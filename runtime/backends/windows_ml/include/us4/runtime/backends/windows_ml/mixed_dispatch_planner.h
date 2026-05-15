#pragma once

#include "us4/runtime/backends/vulkan/vulkan_execution_plan.h"
#include "us4/runtime/backends/windows_ml/layer_offloader.h"
#include "us4/runtime/backends/windows_ml/power_thermal_monitor.h"

#include <string>
#include <vector>

namespace us4::runtime::backends::windows_ml
{

    enum class MixedDispatchTarget
    {
        kGpuPrimary,
        kNpuDense,
        kHostAssist,
        kCpuFallback,
    };

    struct MixedDispatchSlice
    {
        std::string layerName;
        MixedDispatchTarget target = MixedDispatchTarget::kGpuPrimary;
        bool requiresSynchronization = false;
        bool usesWindowsMl = false;
        std::string reason;
    };

    struct MixedDispatchPlan
    {
        bool gpuPrimaryActive = true;
        bool npuDenseActive = false;
        bool hostAssistRequired = false;
        bool cpuFallbackPresent = false;
        bool degradedByPolicy = false;
        std::uint32_t npuDemotionCount = 0;
        PowerDispatchPolicy policy = PowerDispatchPolicy::kNominal;
        std::vector<MixedDispatchSlice> slices;
    };

    [[nodiscard]] std::string ToString(MixedDispatchTarget target);

    class MixedDispatchPlanner
    {
      public:
        [[nodiscard]] static MixedDispatchPlan Build(const vulkan::VulkanExecutionPlan& gpuPlan,
                                                     const WindowsMlExecutionPlan& npuPlan,
                                                     const std::vector<LayerDescriptor>& layers,
                                                     const PowerThermalSnapshot& snapshot = {});
    };

} // namespace us4::runtime::backends::windows_ml
