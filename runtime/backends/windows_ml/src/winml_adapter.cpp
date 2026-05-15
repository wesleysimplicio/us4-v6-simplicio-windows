#include "us4/runtime/backends/windows_ml/winml_adapter.h"

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

    WinMlAdapter::WinMlAdapter(WinMlAdapterOptions options) : options_(options) {}

    bool WinMlAdapter::Initialize(const HardwareCapabilities& capabilities)
    {
        acceleratorName_ = capabilities.npuName;
        npuAvailable_ = capabilities.hasNpu;
        lastIssue_.reset();
        state_ = WinMlAdapterState::kCold;
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
                .message = "Windows ML graph compilation requires NPU opt-in when CPU fallback is disabled.",
                .recoverable = true,
            };
            return false;
        }

        if (plan.optInSatisfied && !npuAvailable_)
        {
            state_ = WinMlAdapterState::kFaulted;
            lastIssue_ = WinMlAdapterIssue{
                .code = "winml.npu-missing",
                .message = "The execution plan targets NPU partitions but no NPU is available.",
                .recoverable = true,
            };
            return false;
        }

        state_ = WinMlAdapterState::kCompiled;
        return true;
    }

    void WinMlAdapter::Reset()
    {
        state_ = WinMlAdapterState::kCold;
        lastIssue_.reset();
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

    std::string WinMlAdapter::DescribeSession() const
    {
        std::ostringstream builder;
        builder << "backend=windows-ml"
                << " accelerator=\"" << acceleratorName_ << "\""
                << " state=" << ToString(state_)
                << " model=\"" << boundModelId_ << "\""
                << " npu_partitions=" << stats_.npuPartitionCount
                << " host_partitions=" << stats_.hostPartitionCount
                << " cpu_fallback_partitions=" << stats_.cpuFallbackPartitionCount
                << " batch_hint=" << stats_.lastBatchHint
                << " context_hint=" << stats_.lastContextHint;
        return builder.str();
    }

} // namespace us4::runtime::backends::windows_ml
