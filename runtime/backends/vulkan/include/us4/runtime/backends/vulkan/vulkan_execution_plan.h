#pragma once

#include "us4/runtime/adapters/adapter_contracts.h"

#include <algorithm>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace us4::runtime::backends::vulkan
{

    enum class VulkanQueueClass
    {
        kDedicatedCompute,
        kDedicatedTransfer,
        kSharedGraphicsCompute,
        kHostVisibleFallback,
    };

    enum class VulkanResidencyClass
    {
        kDeviceLocal,
        kHostVisible,
        kHostCached,
        kStorageBacked,
    };

    enum class VulkanKernelStage
    {
        kPrefill,
        kDecode,
        kKvPageIn,
        kKvPageOut,
        kSpeculativeDraft,
        kMoERouterHostAssist,
        kCpuAttentionFallback,
    };

    struct VulkanDispatchShape
    {
        std::uint32_t workgroupX = 1;
        std::uint32_t workgroupY = 1;
        std::uint32_t workgroupZ = 1;
        std::uint32_t dispatchX = 1;
        std::uint32_t dispatchY = 1;
        std::uint32_t dispatchZ = 1;
    };

    struct VulkanPipelineKey
    {
        std::string shaderName;
        PrecisionMode precision = PrecisionMode::kFp16;
        bool usesSubgroups = false;
        bool usesInt8Weights = false;
        bool usesFp16Accumulators = true;
    };

    struct VulkanExecutionStep
    {
        VulkanKernelStage stage = VulkanKernelStage::kPrefill;
        VulkanQueueClass queueClass = VulkanQueueClass::kDedicatedCompute;
        VulkanResidencyClass residencyClass = VulkanResidencyClass::kDeviceLocal;
        VulkanPipelineKey pipeline;
        VulkanDispatchShape dispatch;
        bool asynchronous = true;
        bool usesTimelineSemaphore = true;
        bool requiresHostVisibleBarrier = false;
    };

    struct VulkanExecutionInputs
    {
        adapters::RuntimeBinding binding;
        SessionRequest request;
        std::string adapterId;
        std::size_t targetBatchSize = 1;
        bool modelSupportsMoE = false;
        bool modelSupportsSpeculative = false;
    };

    struct VulkanExecutionPlan
    {
        adapters::RuntimeBinding binding;
        std::string adapterId;
        bool compilePipelinesAheadOfTime = true;
        bool useTimelineSemaphores = true;
        bool useDescriptorBuffer = false;
        bool usePersistentMappedKv = false;
        bool enableHostVisibleKvSpill = false;
        bool routeMoEOnHost = false;
        bool fallbackToCpuAttention = false;
        std::uint32_t maxConcurrentPrefillBatches = 1;
        std::uint32_t decodeMicroBatchSize = 1;
        std::vector<VulkanExecutionStep> steps;
        std::vector<RuntimeIssue> issues;
    };

    [[nodiscard]] inline VulkanResidencyClass
    ResolveVulkanResidency(const VulkanExecutionInputs& inputs)
    {
        if (inputs.binding.mode == RuntimeMode::kNano)
        {
            return VulkanResidencyClass::kStorageBacked;
        }

        if (inputs.binding.mode == RuntimeMode::kMicro ||
            inputs.binding.mode == RuntimeMode::kUltraLow)
        {
            return VulkanResidencyClass::kHostCached;
        }

        if (inputs.binding.backend.supportsUnifiedMemory ||
            inputs.binding.backend.deviceClass == DeviceClass::kIntegratedGpu)
        {
            return VulkanResidencyClass::kHostVisible;
        }

        return VulkanResidencyClass::kDeviceLocal;
    }

    [[nodiscard]] inline VulkanQueueClass
    ResolveVulkanQueueClass(const VulkanExecutionInputs& inputs)
    {
        if (inputs.binding.mode == RuntimeMode::kNano)
        {
            return VulkanQueueClass::kHostVisibleFallback;
        }

        if (inputs.binding.backend.deviceClass == DeviceClass::kIntegratedGpu)
        {
            return VulkanQueueClass::kSharedGraphicsCompute;
        }

        return VulkanQueueClass::kDedicatedCompute;
    }

    [[nodiscard]] inline bool VulkanPrefersInt8Weights(const VulkanExecutionInputs& inputs)
    {
        return inputs.request.precision == PrecisionMode::kInt8 ||
               inputs.binding.mode == RuntimeMode::kMicro ||
               inputs.binding.mode == RuntimeMode::kNano;
    }

    [[nodiscard]] inline std::uint32_t
    ResolveVulkanDecodeMicroBatchSize(const VulkanExecutionInputs& inputs)
    {
        const std::size_t batchSize = std::max<std::size_t>(inputs.targetBatchSize, 1U);
        if (inputs.binding.backend.deviceClass == DeviceClass::kDiscreteGpu &&
            inputs.binding.mode != RuntimeMode::kNano)
        {
            return static_cast<std::uint32_t>(std::min<std::size_t>(batchSize, 4U));
        }
        return 1U;
    }

    [[nodiscard]] inline VulkanExecutionPlan
    BuildVulkanExecutionPlan(const VulkanExecutionInputs& inputs)
    {
        VulkanExecutionPlan plan{};
        plan.binding = inputs.binding;
        plan.adapterId = inputs.adapterId;
        plan.useTimelineSemaphores = inputs.binding.mode != RuntimeMode::kNano;
        plan.useDescriptorBuffer = inputs.binding.backend.deviceClass == DeviceClass::kDiscreteGpu;
        plan.usePersistentMappedKv = inputs.binding.backend.supportsPagedKv &&
                                     inputs.binding.mode != RuntimeMode::kNano &&
                                     inputs.binding.mode != RuntimeMode::kCpuOnly;
        plan.enableHostVisibleKvSpill = inputs.binding.mode == RuntimeMode::kDegraded ||
                                        inputs.binding.mode == RuntimeMode::kUltraLow ||
                                        inputs.binding.mode == RuntimeMode::kMicro ||
                                        inputs.binding.mode == RuntimeMode::kNano;
        plan.routeMoEOnHost = inputs.modelSupportsMoE && !inputs.binding.backend.supportsMoE;
        plan.fallbackToCpuAttention = inputs.binding.mode == RuntimeMode::kNano;
        plan.maxConcurrentPrefillBatches =
            inputs.binding.backend.deviceClass == DeviceClass::kDiscreteGpu
                ? static_cast<std::uint32_t>(
                      std::min<std::size_t>(std::max<std::size_t>(inputs.targetBatchSize, 1U), 4U))
                : 1U;
        plan.decodeMicroBatchSize = ResolveVulkanDecodeMicroBatchSize(inputs);

        const VulkanResidencyClass baseResidency = ResolveVulkanResidency(inputs);
        const VulkanQueueClass baseQueue = ResolveVulkanQueueClass(inputs);
        const bool usesInt8Weights = VulkanPrefersInt8Weights(inputs);

        plan.steps.push_back(VulkanExecutionStep{
            .stage = VulkanKernelStage::kPrefill,
            .queueClass = baseQueue,
            .residencyClass = baseResidency,
            .pipeline =
                VulkanPipelineKey{
                    .shaderName = "dense_prefill",
                    .precision = inputs.binding.backend.defaultPrecision,
                    .usesSubgroups =
                        inputs.binding.backend.deviceClass != DeviceClass::kIntegratedGpu,
                    .usesInt8Weights = usesInt8Weights,
                    .usesFp16Accumulators = inputs.request.precision != PrecisionMode::kFp32,
                },
            .dispatch =
                VulkanDispatchShape{
                    .workgroupX = 128,
                    .workgroupY = 1,
                    .workgroupZ = 1,
                    .dispatchX = plan.maxConcurrentPrefillBatches,
                    .dispatchY = 1,
                    .dispatchZ = 1,
                },
            .asynchronous = true,
            .usesTimelineSemaphore = plan.useTimelineSemaphores,
            .requiresHostVisibleBarrier = baseResidency != VulkanResidencyClass::kDeviceLocal,
        });

        if (plan.usePersistentMappedKv)
        {
            plan.steps.push_back(VulkanExecutionStep{
                .stage = VulkanKernelStage::kKvPageIn,
                .queueClass = inputs.binding.backend.deviceClass == DeviceClass::kDiscreteGpu
                                  ? VulkanQueueClass::kDedicatedTransfer
                                  : baseQueue,
                .residencyClass = baseResidency,
                .pipeline =
                    VulkanPipelineKey{
                        .shaderName = "kv_page_in",
                        .precision = PrecisionMode::kFp16,
                        .usesSubgroups = false,
                        .usesInt8Weights = false,
                        .usesFp16Accumulators = true,
                    },
                .dispatch =
                    VulkanDispatchShape{
                        .workgroupX = 64,
                        .workgroupY = 1,
                        .workgroupZ = 1,
                        .dispatchX = 1,
                        .dispatchY = 1,
                        .dispatchZ = 1,
                    },
                .asynchronous = true,
                .usesTimelineSemaphore = plan.useTimelineSemaphores,
                .requiresHostVisibleBarrier = baseResidency == VulkanResidencyClass::kHostVisible ||
                                              baseResidency == VulkanResidencyClass::kHostCached,
            });
        }

        if (inputs.request.enableSpeculative && inputs.modelSupportsSpeculative &&
            inputs.binding.backend.supportsSpeculative)
        {
            plan.steps.push_back(VulkanExecutionStep{
                .stage = VulkanKernelStage::kSpeculativeDraft,
                .queueClass = baseQueue,
                .residencyClass = baseResidency,
                .pipeline =
                    VulkanPipelineKey{
                        .shaderName = "speculative_draft",
                        .precision = inputs.binding.backend.defaultPrecision,
                        .usesSubgroups = true,
                        .usesInt8Weights = usesInt8Weights,
                        .usesFp16Accumulators = true,
                    },
                .dispatch =
                    VulkanDispatchShape{
                        .workgroupX = 64,
                        .workgroupY = 1,
                        .workgroupZ = 1,
                        .dispatchX = plan.decodeMicroBatchSize,
                        .dispatchY = 1,
                        .dispatchZ = 1,
                    },
                .asynchronous = true,
                .usesTimelineSemaphore = plan.useTimelineSemaphores,
                .requiresHostVisibleBarrier = false,
            });
        }

        if (plan.routeMoEOnHost)
        {
            plan.steps.push_back(VulkanExecutionStep{
                .stage = VulkanKernelStage::kMoERouterHostAssist,
                .queueClass = VulkanQueueClass::kHostVisibleFallback,
                .residencyClass = VulkanResidencyClass::kHostCached,
                .pipeline =
                    VulkanPipelineKey{
                        .shaderName = "moe_router_host_assist",
                        .precision = PrecisionMode::kFp32,
                        .usesSubgroups = false,
                        .usesInt8Weights = false,
                        .usesFp16Accumulators = false,
                    },
                .dispatch =
                    VulkanDispatchShape{
                        .workgroupX = 1,
                        .workgroupY = 1,
                        .workgroupZ = 1,
                        .dispatchX = 1,
                        .dispatchY = 1,
                        .dispatchZ = 1,
                    },
                .asynchronous = false,
                .usesTimelineSemaphore = false,
                .requiresHostVisibleBarrier = true,
            });

            plan.issues.push_back(
                {"vulkan.moe.host_route",
                 "MoE routing stays on the host path for cross-vendor Vulkan fallback.", true});
        }

        plan.steps.push_back(VulkanExecutionStep{
            .stage = plan.fallbackToCpuAttention ? VulkanKernelStage::kCpuAttentionFallback
                                                 : VulkanKernelStage::kDecode,
            .queueClass =
                plan.fallbackToCpuAttention ? VulkanQueueClass::kHostVisibleFallback : baseQueue,
            .residencyClass =
                plan.fallbackToCpuAttention ? VulkanResidencyClass::kHostCached : baseResidency,
            .pipeline =
                VulkanPipelineKey{
                    .shaderName =
                        plan.fallbackToCpuAttention ? "cpu_attention_fallback" : "dense_decode",
                    .precision = plan.fallbackToCpuAttention
                                     ? PrecisionMode::kFp32
                                     : inputs.binding.backend.defaultPrecision,
                    .usesSubgroups =
                        !plan.fallbackToCpuAttention &&
                        inputs.binding.backend.deviceClass == DeviceClass::kDiscreteGpu,
                    .usesInt8Weights = !plan.fallbackToCpuAttention && usesInt8Weights,
                    .usesFp16Accumulators = !plan.fallbackToCpuAttention,
                },
            .dispatch =
                VulkanDispatchShape{
                    .workgroupX = 64,
                    .workgroupY = 1,
                    .workgroupZ = 1,
                    .dispatchX = plan.decodeMicroBatchSize,
                    .dispatchY = 1,
                    .dispatchZ = 1,
                },
            .asynchronous = !plan.fallbackToCpuAttention,
            .usesTimelineSemaphore = !plan.fallbackToCpuAttention && plan.useTimelineSemaphores,
            .requiresHostVisibleBarrier = plan.fallbackToCpuAttention ||
                                          baseResidency == VulkanResidencyClass::kHostVisible ||
                                          baseResidency == VulkanResidencyClass::kHostCached,
        });

        if (plan.enableHostVisibleKvSpill)
        {
            plan.steps.push_back(VulkanExecutionStep{
                .stage = VulkanKernelStage::kKvPageOut,
                .queueClass = VulkanQueueClass::kDedicatedTransfer,
                .residencyClass = VulkanResidencyClass::kStorageBacked,
                .pipeline =
                    VulkanPipelineKey{
                        .shaderName = "kv_page_out",
                        .precision = PrecisionMode::kFp16,
                        .usesSubgroups = false,
                        .usesInt8Weights = false,
                        .usesFp16Accumulators = true,
                    },
                .dispatch =
                    VulkanDispatchShape{
                        .workgroupX = 64,
                        .workgroupY = 1,
                        .workgroupZ = 1,
                        .dispatchX = 1,
                        .dispatchY = 1,
                        .dispatchZ = 1,
                    },
                .asynchronous = true,
                .usesTimelineSemaphore = plan.useTimelineSemaphores,
                .requiresHostVisibleBarrier = true,
            });
        }

        if (inputs.request.requireDeterministic)
        {
            plan.issues.push_back(
                {"vulkan.determinism.soft_block",
                 "Vulkan fallback keeps a soft determinism warning because queue timing may vary "
                 "across vendors.",
                 true});
        }

        if (inputs.request.allowNpu)
        {
            plan.issues.push_back(
                {"vulkan.npu.bypass",
                 "The Vulkan fallback plan ignores NPU opt-in and keeps execution on the GPU/CPU "
                 "hybrid path.",
                 true});
        }

        if (plan.fallbackToCpuAttention)
        {
            plan.issues.push_back(
                {"vulkan.nano.cpu_attention",
                 "Nano mode demotes attention decode to the CPU path to preserve correctness on "
                 "the smallest budgets.",
                 true});
        }

        return plan;
    }

    [[nodiscard]] inline std::string_view ToString(const VulkanKernelStage stage)
    {
        switch (stage)
        {
        case VulkanKernelStage::kPrefill:
            return "prefill";
        case VulkanKernelStage::kDecode:
            return "decode";
        case VulkanKernelStage::kKvPageIn:
            return "kv-page-in";
        case VulkanKernelStage::kKvPageOut:
            return "kv-page-out";
        case VulkanKernelStage::kSpeculativeDraft:
            return "speculative-draft";
        case VulkanKernelStage::kMoERouterHostAssist:
            return "moe-router-host-assist";
        case VulkanKernelStage::kCpuAttentionFallback:
            return "cpu-attention-fallback";
        }
        return "unknown";
    }

} // namespace us4::runtime::backends::vulkan
