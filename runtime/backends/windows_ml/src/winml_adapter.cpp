#include "us4/runtime/backends/windows_ml/winml_adapter.h"

#include <algorithm>
#include <sstream>

namespace us4::runtime::backends::windows_ml
{

    std::string ToString(const WinMlAdapterState state)
    {
        switch (state)
        {
        case WinMlAdapterState::kCold:
            return "cold";
        case WinMlAdapterState::kReady:
            return "ready";
        case WinMlAdapterState::kCompiled:
            return "compiled";
        case WinMlAdapterState::kFaulted:
            return "faulted";
        }

        return "unknown";
    }

    std::string ToString(const WinMlCompileTarget target)
    {
        switch (target)
        {
        case WinMlCompileTarget::kNpu:
            return "npu";
        case WinMlCompileTarget::kCpuFallback:
            return "cpu-fallback";
        }

        return "unknown";
    }

    WinMlAdapter::WinMlAdapter(WinMlAdapterOptions options) : options_(options) {}

    bool WinMlAdapter::Initialize(const HardwareCapabilities& capabilities)
    {
        acceleratorName_ = capabilities.npuName;
        npuAvailable_ = capabilities.hasNpu;
        lastIssue_.reset();
        state_ = WinMlAdapterState::kCold;
        sessionArtifact_.reset();
        boundModelId_.clear();

        if (!capabilities.hasNpu && !options_.allowCpuFallback)
        {
            state_ = WinMlAdapterState::kFaulted;
            lastIssue_ = WinMlAdapterIssue{
                .code = "winml.npu-unavailable",
                .message = "Windows ML adapter requires an NPU or explicit CPU fallback support.",
                .recoverable = true,
            };
            return false;
        }

        ++stats_.initializeCount;
        state_ = WinMlAdapterState::kReady;
        return true;
    }

    bool WinMlAdapter::CompileGraph(std::string_view modelId, const WindowsMlExecutionPlan& plan)
    {
        if (state_ != WinMlAdapterState::kReady && state_ != WinMlAdapterState::kCompiled)
        {
            state_ = WinMlAdapterState::kFaulted;
            lastIssue_ = WinMlAdapterIssue{
                .code = "winml.adapter-not-ready",
                .message = "Initialize the Windows ML adapter before compiling a graph.",
                .recoverable = true,
            };
            return false;
        }

        if (plan.binding.backend.kind != BackendKind::kWindowsMl)
        {
            state_ = WinMlAdapterState::kFaulted;
            lastIssue_ = WinMlAdapterIssue{
                .code = "winml.backend-kind-mismatch",
                .message = "The supplied execution plan does not target Windows ML.",
                .recoverable = false,
            };
            return false;
        }

        boundModelId_ = std::string(modelId);
        stats_.npuPartitionCount = 0;
        stats_.hostPartitionCount = 0;
        stats_.cpuFallbackPartitionCount = 0;
        sessionArtifact_.reset();

        bool hostAssistRequired = false;
        bool requiresStaticShapes = false;

        for (const auto& partition : plan.partitions)
        {
            if (partition.executesOnNpu)
            {
                ++stats_.npuPartitionCount;
            }
            else
            {
                ++stats_.hostPartitionCount;
            }

            if (partition.kind == WindowsMlPartitionKind::kCpuFallback)
            {
                ++stats_.cpuFallbackPartitionCount;
            }

            hostAssistRequired = hostAssistRequired || partition.requiresHostSynchronization;
            requiresStaticShapes = requiresStaticShapes || partition.requiresStaticShapes;
        }

        stats_.lastBatchHint = plan.batchSizeHint;
        stats_.lastContextHint = plan.contextTokenHint;
        ++stats_.compileCount;
        lastIssue_.reset();

        if (!plan.optInSatisfied && !options_.allowCpuFallback)
        {
            state_ = WinMlAdapterState::kFaulted;
            lastIssue_ = WinMlAdapterIssue{
                .code = "winml.opt-in-required",
                .message = "Windows ML graph compilation requires NPU opt-in when CPU fallback is "
                           "disabled.",
                .recoverable = true,
            };
            return false;
        }

        WinMlCompileTarget compileTarget = WinMlCompileTarget::kCpuFallback;
        std::string fallbackReason;
        if (plan.optInSatisfied && npuAvailable_)
        {
            compileTarget = WinMlCompileTarget::kNpu;
        }
        else if (!plan.optInSatisfied)
        {
            fallbackReason = "opt-in-required";
        }
        else if (!npuAvailable_)
        {
            fallbackReason = "npu-unavailable";
        }

        if (plan.optInSatisfied && !npuAvailable_)
        {
            if (!options_.allowCpuFallback)
            {
                state_ = WinMlAdapterState::kFaulted;
                lastIssue_ = WinMlAdapterIssue{
                    .code = "winml.npu-missing",
                    .message = "The execution plan targets NPU partitions but no NPU is available.",
                    .recoverable = true,
                };
                return false;
            }

            stats_.hostPartitionCount = static_cast<std::uint32_t>(plan.partitions.size());
            stats_.npuPartitionCount = 0;
            stats_.cpuFallbackPartitionCount =
                std::max<std::uint32_t>(1U, stats_.cpuFallbackPartitionCount);
            lastIssue_ = WinMlAdapterIssue{
                .code = "winml.npu-missing-fallback",
                .message =
                    "Windows ML compiled a CPU fallback session because no NPU is available.",
                .recoverable = true,
            };
        }

        sessionArtifact_ = WinMlSessionArtifact{
            .modelId = boundModelId_,
            .compileTarget = compileTarget,
            .batchHint = stats_.lastBatchHint,
            .contextHint = stats_.lastContextHint,
            .npuPartitionCount = stats_.npuPartitionCount,
            .hostPartitionCount = stats_.hostPartitionCount,
            .cpuFallbackPartitionCount = stats_.cpuFallbackPartitionCount,
            .cpuFallbackArmed = compileTarget == WinMlCompileTarget::kCpuFallback,
            .hostAssistRequired = hostAssistRequired,
            .reusableGraph = options_.preferStaticShapes,
            .requiresStaticShapes = requiresStaticShapes,
            .fallbackReason = std::move(fallbackReason),
            .cacheKey = boundModelId_ + "|" + ToString(compileTarget) + "|b" +
                        std::to_string(stats_.lastBatchHint) + "|c" +
                        std::to_string(stats_.lastContextHint),
        };
        state_ = WinMlAdapterState::kCompiled;
        return true;
    }

    void WinMlAdapter::Reset()
    {
        state_ = WinMlAdapterState::kCold;
        lastIssue_.reset();
        sessionArtifact_.reset();
        boundModelId_.clear();
        acceleratorName_.clear();
        npuAvailable_ = false;
    }

    WinMlAdapterState WinMlAdapter::State() const
    {
        return state_;
    }

    const WinMlAdapterOptions& WinMlAdapter::Options() const
    {
        return options_;
    }

    const WinMlCompileStats& WinMlAdapter::Stats() const
    {
        return stats_;
    }

    std::optional<WinMlAdapterIssue> WinMlAdapter::LastIssue() const
    {
        return lastIssue_;
    }

    std::optional<WinMlSessionArtifact> WinMlAdapter::SessionArtifact() const
    {
        return sessionArtifact_;
    }

    std::string WinMlAdapter::DescribeSession() const
    {
        std::ostringstream builder;
        builder << "backend=windows-ml"
                << " accelerator=\"" << acceleratorName_ << "\""
                << " state=" << ToString(state_) << " model=\"" << boundModelId_ << "\""
                << " npu_partitions=" << stats_.npuPartitionCount
                << " host_partitions=" << stats_.hostPartitionCount
                << " cpu_fallback_partitions=" << stats_.cpuFallbackPartitionCount
                << " batch_hint=" << stats_.lastBatchHint
                << " context_hint=" << stats_.lastContextHint;
        if (sessionArtifact_.has_value())
        {
            builder << " compile_target=" << ToString(sessionArtifact_->compileTarget)
                    << " reusable_graph=" << (sessionArtifact_->reusableGraph ? "yes" : "no")
                    << " cache_key=\"" << sessionArtifact_->cacheKey << "\"";
            if (!sessionArtifact_->fallbackReason.empty())
            {
                builder << " fallback_reason=\"" << sessionArtifact_->fallbackReason << '"';
            }
        }
        return builder.str();
    }

} // namespace us4::runtime::backends::windows_ml
