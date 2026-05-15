#pragma once

#include "us4/runtime/backends/hardware_probe.h"
#include "us4/runtime/backends/vulkan/vulkan_execution_plan.h"

#include <optional>
#include <string>
#include <string_view>

namespace us4::runtime::backends::vulkan
{

    enum class VulkanContextState
    {
        kCold,
        kReady,
        kBound,
        kFaulted,
    };

    struct VulkanContextOptions
    {
        bool preferIntegratedGpu = false;
        bool enableValidationLayer = false;
        bool allowDescriptorBuffer = true;
        bool preferAsyncTransfers = true;
    };

    struct VulkanDescriptorArena
    {
        std::uint32_t setCount = 0;
        std::size_t persistentBytes = 0;
        std::size_t transientBytes = 0;
    };

    struct VulkanQueueBinding
    {
        VulkanQueueClass queueClass = VulkanQueueClass::kDedicatedCompute;
        std::uint32_t familyIndex = 0;
        std::uint32_t queueIndex = 0;
        bool asyncCapable = true;
    };

    struct VulkanContextStats
    {
        std::uint32_t initializeCount = 0;
        std::uint32_t bindCount = 0;
        std::uint32_t descriptorRebuildCount = 0;
        std::uint32_t pipelineStageCount = 0;
        std::size_t persistentBytes = 0;
        std::size_t transientBytes = 0;
    };

    struct VulkanContextIssue
    {
        std::string code;
        std::string message;
        bool recoverable = true;
    };

    [[nodiscard]] std::string ToString(VulkanContextState state);

    class VulkanContext
    {
      public:
        explicit VulkanContext(VulkanContextOptions options = {});

        [[nodiscard]] bool Initialize(const HardwareCapabilities& capabilities);
        [[nodiscard]] bool BindExecutionPlan(std::string_view modelId,
                                             const VulkanExecutionPlan& plan);
        void Reset();

        [[nodiscard]] const VulkanContextOptions& Options() const;
        [[nodiscard]] VulkanContextState State() const;
        [[nodiscard]] const VulkanQueueBinding& Queue() const;
        [[nodiscard]] const VulkanDescriptorArena& DescriptorArena() const;
        [[nodiscard]] const VulkanContextStats& Stats() const;
        [[nodiscard]] std::optional<VulkanContextIssue> LastIssue() const;
        [[nodiscard]] std::string DescribeSession() const;

      private:
        VulkanContextOptions options_;
        VulkanContextState state_ = VulkanContextState::kCold;
        VulkanQueueBinding queue_{};
        VulkanDescriptorArena descriptorArena_{};
        VulkanContextStats stats_{};
        std::optional<VulkanContextIssue> lastIssue_;
        std::string adapterName_;
        std::string boundModelId_;
        bool descriptorBufferEnabled_ = false;
        bool validationLayerEnabled_ = false;
    };

} // namespace us4::runtime::backends::vulkan
