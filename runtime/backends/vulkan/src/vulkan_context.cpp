#include "us4/runtime/backends/vulkan/vulkan_context.h"

#include <algorithm>
#include <sstream>

namespace us4::runtime::backends::vulkan
{

    namespace
    {
        VulkanQueueBinding ResolveQueueBinding(const HardwareCapabilities& capabilities,
                                              const VulkanContextOptions& options)
        {
            VulkanQueueBinding queue{};
            if (capabilities.primaryAdapterClass == DeviceClass::kIntegratedGpu ||
                options.preferIntegratedGpu)
            {
                queue.queueClass = VulkanQueueClass::kSharedGraphicsCompute;
                queue.familyIndex = 1;
                queue.queueIndex = 0;
                queue.asyncCapable = false;
                return queue;
            }

            queue.queueClass = VulkanQueueClass::kDedicatedCompute;
            queue.familyIndex = 2;
            queue.queueIndex = 0;
            queue.asyncCapable = options.preferAsyncTransfers;
            return queue;
        }

        std::size_t EstimatePersistentBytes(const VulkanExecutionPlan& plan)
        {
            return static_cast<std::size_t>(plan.steps.size()) * 256ULL * 1024ULL +
                   (plan.usePersistentMappedKv ? 2ULL * 1024ULL * 1024ULL : 0ULL) +
                   (plan.useDescriptorBuffer ? 512ULL * 1024ULL : 0ULL);
        }

        std::size_t EstimateTransientBytes(const VulkanExecutionPlan& plan)
        {
            return static_cast<std::size_t>(plan.maxConcurrentPrefillBatches) * 512ULL * 1024ULL +
                   static_cast<std::size_t>(plan.decodeMicroBatchSize) * 256ULL * 1024ULL;
        }
    } // namespace

    std::string ToString(const VulkanContextState state)
    {
        switch (state)
        {
        case VulkanContextState::kCold:
            return "cold";
        case VulkanContextState::kReady:
            return "ready";
        case VulkanContextState::kBound:
            return "bound";
        case VulkanContextState::kFaulted:
            return "faulted";
        }

        return "unknown";
    }

    VulkanContext::VulkanContext(VulkanContextOptions options) : options_(options) {}

    bool VulkanContext::Initialize(const HardwareCapabilities& capabilities)
    {
        lastIssue_.reset();
        state_ = VulkanContextState::kCold;
        boundModelId_.clear();
        descriptorArena_ = {};

        if (!capabilities.hasVulkan)
        {
            state_ = VulkanContextState::kFaulted;
            lastIssue_ = VulkanContextIssue{
                .code = "vulkan-unavailable",
                .message = "Vulkan is not available on this host.",
                .recoverable = true,
            };
            return false;
        }

        adapterName_ = capabilities.primaryAdapterName;
        queue_ = ResolveQueueBinding(capabilities, options_);
        descriptorBufferEnabled_ = options_.allowDescriptorBuffer &&
                                   capabilities.primaryAdapterClass == DeviceClass::kDiscreteGpu;
        validationLayerEnabled_ = options_.enableValidationLayer;
        ++stats_.initializeCount;
        state_ = VulkanContextState::kReady;
        return true;
    }

    bool VulkanContext::BindExecutionPlan(std::string_view modelId, const VulkanExecutionPlan& plan)
    {
        if (state_ != VulkanContextState::kReady && state_ != VulkanContextState::kBound)
        {
            state_ = VulkanContextState::kFaulted;
            lastIssue_ = VulkanContextIssue{
                .code = "context-not-ready",
                .message = "Initialize the Vulkan context before binding an execution plan.",
                .recoverable = true,
            };
            return false;
        }

        if (plan.binding.backend.kind != BackendKind::kVulkan)
        {
            state_ = VulkanContextState::kFaulted;
            lastIssue_ = VulkanContextIssue{
                .code = "backend-kind-mismatch",
                .message = "The supplied execution plan is not a Vulkan plan.",
                .recoverable = false,
            };
            return false;
        }

        boundModelId_ = std::string(modelId);
        descriptorArena_.setCount = std::max<std::uint32_t>(
            1U, static_cast<std::uint32_t>(plan.steps.size() * 2U));
        descriptorArena_.persistentBytes = EstimatePersistentBytes(plan);
        descriptorArena_.transientBytes = EstimateTransientBytes(plan);
        stats_.persistentBytes = descriptorArena_.persistentBytes;
        stats_.transientBytes = descriptorArena_.transientBytes;
        stats_.pipelineStageCount = static_cast<std::uint32_t>(plan.steps.size());
        ++stats_.bindCount;
        if (plan.useDescriptorBuffer != descriptorBufferEnabled_)
        {
            ++stats_.descriptorRebuildCount;
        }
        lastIssue_.reset();
        state_ = VulkanContextState::kBound;
        return true;
    }

    void VulkanContext::Reset()
    {
        state_ = VulkanContextState::kCold;
        queue_ = {};
        descriptorArena_ = {};
        lastIssue_.reset();
        boundModelId_.clear();
        adapterName_.clear();
        descriptorBufferEnabled_ = false;
        validationLayerEnabled_ = false;
    }

    const VulkanContextOptions& VulkanContext::Options() const
    {
        return options_;
    }

    VulkanContextState VulkanContext::State() const
    {
        return state_;
    }

    const VulkanQueueBinding& VulkanContext::Queue() const
    {
        return queue_;
    }

    const VulkanDescriptorArena& VulkanContext::DescriptorArena() const
    {
        return descriptorArena_;
    }

    const VulkanContextStats& VulkanContext::Stats() const
    {
        return stats_;
    }

    std::optional<VulkanContextIssue> VulkanContext::LastIssue() const
    {
        return lastIssue_;
    }

    std::string VulkanContext::DescribeSession() const
    {
        std::ostringstream builder;
        builder << "backend=vulkan"
                << " adapter=\"" << adapterName_ << "\""
                << " state=" << ToString(state_)
                << " model=\"" << boundModelId_ << "\""
                << " queue_class=" << static_cast<int>(queue_.queueClass)
                << " descriptor_sets=" << descriptorArena_.setCount
                << " persistent_bytes=" << descriptorArena_.persistentBytes
                << " transient_bytes=" << descriptorArena_.transientBytes
                << " descriptor_buffer=" << (descriptorBufferEnabled_ ? "on" : "off")
                << " validation=" << (validationLayerEnabled_ ? "on" : "off");
        return builder.str();
    }

} // namespace us4::runtime::backends::vulkan
