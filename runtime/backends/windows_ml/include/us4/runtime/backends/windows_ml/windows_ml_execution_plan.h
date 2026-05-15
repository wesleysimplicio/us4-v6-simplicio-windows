#pragma once

#include "us4/runtime/adapters/adapter_contracts.h"

#include <algorithm>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace us4::runtime::backends::windows_ml
{

    enum class WindowsMlPartitionKind
    {
        kGraphCompile,
        kPrefillEncoder,
        kDecodeProjection,
        kKvCompression,
        kHostAssist,
        kCpuFallback,
    };

    enum class WindowsMlResidencyClass
    {
        kNpuTileMemory,
        kSharedHostNpu,
        kHostPinned,
        kStorageBacked,
    };

    struct WindowsMlCompiledPartition
    {
        WindowsMlPartitionKind kind = WindowsMlPartitionKind::kGraphCompile;
        WindowsMlResidencyClass residencyClass = WindowsMlResidencyClass::kSharedHostNpu;
        PrecisionMode precision = PrecisionMode::kInt8;
        std::uint32_t batchSizeHint = 1;
        std::uint32_t contextTokenHint = 0;
        bool executesOnNpu = false;
        bool requiresStaticShapes = false;
        bool requiresHostSynchronization = false;
    };

    struct WindowsMlExecutionInputs
    {
        adapters::RuntimeBinding binding;
        SessionRequest request;
        std::string adapterId;
        std::size_t targetBatchSize = 1;
        bool modelSupportsMoE = false;
        bool modelSupportsSpeculative = false;
        bool modelSupportsVision = false;
    };

    struct WindowsMlExecutionPlan
    {
        adapters::RuntimeBinding binding;
        std::string adapterId;
        bool optInSatisfied = false;
        bool compileGraphOnce = true;
        bool useNpuResidency = false;
        bool useInt8Weights = true;
        bool allowGpuFallback = true;
        bool offloadPrefill = false;
        bool offloadDecode = false;
        bool compressKvOnHost = false;
        bool routeMoEOnHost = false;
        std::uint32_t batchSizeHint = 1;
        std::uint32_t contextTokenHint = 0;
        std::vector<WindowsMlCompiledPartition> partitions;
        std::vector<RuntimeIssue> issues;
    };

    [[nodiscard]] inline bool WindowsMlRequestTargetsNpu(const WindowsMlExecutionInputs& inputs)
    {
        return inputs.request.preferredBackend.empty() ||
               inputs.request.preferredBackend == "auto" ||
               inputs.request.preferredBackend == "windows-ml" ||
               inputs.request.preferredBackend == "npu" ||
               inputs.binding.backend.kind == BackendKind::kWindowsMl;
    }

    [[nodiscard]] inline WindowsMlResidencyClass
    ResolveWindowsMlResidency(const WindowsMlExecutionInputs& inputs)
    {
        if (inputs.binding.mode == RuntimeMode::kNano)
        {
            return WindowsMlResidencyClass::kStorageBacked;
        }

        if (inputs.binding.mode == RuntimeMode::kMicro ||
            inputs.binding.mode == RuntimeMode::kUltraLow)
        {
            return WindowsMlResidencyClass::kHostPinned;
        }

        return WindowsMlResidencyClass::kSharedHostNpu;
    }

    [[nodiscard]] inline bool WindowsMlRequiresInt8Weights(const WindowsMlExecutionInputs& inputs)
    {
        return inputs.request.precision != PrecisionMode::kFp32;
    }

    [[nodiscard]] inline std::uint32_t
    ResolveWindowsMlBatchSize(const WindowsMlExecutionInputs& inputs)
    {
        const std::size_t requested = std::max<std::size_t>(inputs.targetBatchSize, 1U);
        return static_cast<std::uint32_t>(std::min<std::size_t>(requested, 2U));
    }

    [[nodiscard]] inline WindowsMlExecutionPlan
    BuildWindowsMlExecutionPlan(const WindowsMlExecutionInputs& inputs)
    {
        WindowsMlExecutionPlan plan{};
        plan.binding = inputs.binding;
        plan.adapterId = inputs.adapterId;
        plan.optInSatisfied = inputs.binding.allowNpu && WindowsMlRequestTargetsNpu(inputs);
        plan.useNpuResidency = plan.optInSatisfied;
        plan.useInt8Weights = WindowsMlRequiresInt8Weights(inputs);
        plan.batchSizeHint = ResolveWindowsMlBatchSize(inputs);
        plan.contextTokenHint = std::min<std::uint32_t>(
            inputs.request.maxContextTokens, inputs.binding.backend.maxContextTokensHint == 0
                                                 ? inputs.request.maxContextTokens
                                                 : inputs.binding.backend.maxContextTokensHint);
        plan.offloadPrefill = plan.optInSatisfied && inputs.binding.mode != RuntimeMode::kNano;
        plan.offloadDecode =
            plan.optInSatisfied && inputs.binding.mode != RuntimeMode::kNano &&
            (inputs.request.preferLowLatency || inputs.binding.mode == RuntimeMode::kUltraLow ||
             inputs.binding.mode == RuntimeMode::kMicro);
        plan.compressKvOnHost = inputs.binding.mode == RuntimeMode::kMicro ||
                                inputs.binding.mode == RuntimeMode::kNano ||
                                inputs.request.maxContextTokens > 4096U;
        plan.routeMoEOnHost = inputs.modelSupportsMoE;

        const WindowsMlResidencyClass baseResidency = ResolveWindowsMlResidency(inputs);

        plan.partitions.push_back(WindowsMlCompiledPartition{
            .kind = WindowsMlPartitionKind::kGraphCompile,
            .residencyClass = baseResidency,
            .precision = plan.useInt8Weights ? PrecisionMode::kInt8 : PrecisionMode::kFp32,
            .batchSizeHint = plan.batchSizeHint,
            .contextTokenHint = plan.contextTokenHint,
            .executesOnNpu = plan.optInSatisfied,
            .requiresStaticShapes = true,
            .requiresHostSynchronization = false,
        });

        if (plan.offloadPrefill)
        {
            plan.partitions.push_back(WindowsMlCompiledPartition{
                .kind = WindowsMlPartitionKind::kPrefillEncoder,
                .residencyClass = baseResidency,
                .precision = plan.useInt8Weights ? PrecisionMode::kInt8 : PrecisionMode::kFp16,
                .batchSizeHint = plan.batchSizeHint,
                .contextTokenHint = plan.contextTokenHint,
                .executesOnNpu = true,
                .requiresStaticShapes = true,
                .requiresHostSynchronization = false,
            });
        }

        if (plan.offloadDecode)
        {
            plan.partitions.push_back(WindowsMlCompiledPartition{
                .kind = WindowsMlPartitionKind::kDecodeProjection,
                .residencyClass = baseResidency,
                .precision = plan.useInt8Weights ? PrecisionMode::kInt8 : PrecisionMode::kFp16,
                .batchSizeHint = 1,
                .contextTokenHint = std::min<std::uint32_t>(plan.contextTokenHint, 2048U),
                .executesOnNpu = true,
                .requiresStaticShapes = false,
                .requiresHostSynchronization = false,
            });
        }

        if (plan.compressKvOnHost)
        {
            plan.partitions.push_back(WindowsMlCompiledPartition{
                .kind = WindowsMlPartitionKind::kKvCompression,
                .residencyClass = WindowsMlResidencyClass::kHostPinned,
                .precision = PrecisionMode::kInt8,
                .batchSizeHint = 1,
                .contextTokenHint = plan.contextTokenHint,
                .executesOnNpu = false,
                .requiresStaticShapes = false,
                .requiresHostSynchronization = true,
            });
        }

        if (plan.routeMoEOnHost || inputs.modelSupportsSpeculative || inputs.modelSupportsVision)
        {
            plan.partitions.push_back(WindowsMlCompiledPartition{
                .kind = WindowsMlPartitionKind::kHostAssist,
                .residencyClass = WindowsMlResidencyClass::kHostPinned,
                .precision = PrecisionMode::kFp32,
                .batchSizeHint = 1,
                .contextTokenHint = std::min<std::uint32_t>(plan.contextTokenHint, 1024U),
                .executesOnNpu = false,
                .requiresStaticShapes = false,
                .requiresHostSynchronization = true,
            });
        }

        if (!plan.optInSatisfied)
        {
            plan.partitions.push_back(WindowsMlCompiledPartition{
                .kind = WindowsMlPartitionKind::kCpuFallback,
                .residencyClass = WindowsMlResidencyClass::kHostPinned,
                .precision = PrecisionMode::kFp32,
                .batchSizeHint = 1,
                .contextTokenHint = plan.contextTokenHint,
                .executesOnNpu = false,
                .requiresStaticShapes = false,
                .requiresHostSynchronization = true,
            });

            plan.issues.push_back({"windows_ml.opt_in_required",
                                   "Windows ML stays in hybrid fallback mode until the session "
                                   "explicitly opts into NPU execution.",
                                   true});
        }

        if (plan.routeMoEOnHost)
        {
            plan.issues.push_back({"windows_ml.moe.host_route",
                                   "MoE expert routing remains on the host while Windows ML "
                                   "covers the dense partitions.",
                                   true});
        }

        if (inputs.modelSupportsSpeculative)
        {
            plan.issues.push_back(
                {"windows_ml.speculative.host_assist",
                 "Speculative decoding still requires host assist on the current Windows ML/NPU "
                 "path.",
                 true});
        }

        if (inputs.modelSupportsVision)
        {
            plan.issues.push_back(
                {"windows_ml.vision.host_assist",
                 "Vision pre-processing remains on the host path before Windows ML graph "
                 "execution.",
                 true});
        }

        if (inputs.request.requireDeterministic)
        {
            plan.issues.push_back({"windows_ml.determinism.soft_block",
                                   "Windows ML keeps a soft determinism warning until compiled "
                                   "partition replay is pinned per device family.",
                                   true});
        }

        return plan;
    }

    [[nodiscard]] inline std::string_view ToString(const WindowsMlPartitionKind kind)
    {
        switch (kind)
        {
        case WindowsMlPartitionKind::kGraphCompile:
            return "graph-compile";
        case WindowsMlPartitionKind::kPrefillEncoder:
            return "prefill-encoder";
        case WindowsMlPartitionKind::kDecodeProjection:
            return "decode-projection";
        case WindowsMlPartitionKind::kKvCompression:
            return "kv-compression";
        case WindowsMlPartitionKind::kHostAssist:
            return "host-assist";
        case WindowsMlPartitionKind::kCpuFallback:
            return "cpu-fallback";
        }
        return "unknown";
    }

} // namespace us4::runtime::backends::windows_ml
