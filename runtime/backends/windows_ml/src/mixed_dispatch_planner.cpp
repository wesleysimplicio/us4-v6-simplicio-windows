#include "us4/runtime/backends/windows_ml/mixed_dispatch_planner.h"

namespace us4::runtime::backends::windows_ml
{

    namespace
    {
        MixedDispatchTarget ResolveTarget(const LayerOffloadDecision& decision)
        {
            switch (decision.target)
            {
            case LayerExecutionTarget::kNpu:
                return MixedDispatchTarget::kNpuDense;
            case LayerExecutionTarget::kHostAssist:
                return MixedDispatchTarget::kHostAssist;
            case LayerExecutionTarget::kCpu:
                return MixedDispatchTarget::kCpuFallback;
            case LayerExecutionTarget::kGpu:
                return MixedDispatchTarget::kGpuPrimary;
            }

            return MixedDispatchTarget::kGpuPrimary;
        }
    } // namespace

    std::string ToString(const MixedDispatchTarget target)
    {
        switch (target)
        {
        case MixedDispatchTarget::kGpuPrimary:
            return "gpu-primary";
        case MixedDispatchTarget::kNpuDense:
            return "npu-dense";
        case MixedDispatchTarget::kHostAssist:
            return "host-assist";
        case MixedDispatchTarget::kCpuFallback:
            return "cpu-fallback";
        }

        return "unknown";
    }

    MixedDispatchPlan MixedDispatchPlanner::Build(const vulkan::VulkanExecutionPlan& gpuPlan,
                                                  const WindowsMlExecutionPlan& npuPlan,
                                                  const std::vector<LayerDescriptor>& layers)
    {
        MixedDispatchPlan plan{};
        plan.gpuPrimaryActive = !gpuPlan.steps.empty();
        plan.npuDenseActive = npuPlan.optInSatisfied;
        const auto offloadDecisions = LayerOffloader::BuildDispatchTable(npuPlan, layers);

        plan.slices.reserve(offloadDecisions.size());
        for (const auto& decision : offloadDecisions)
        {
            MixedDispatchSlice slice{
                .layerName = decision.layerName,
                .target = ResolveTarget(decision),
                .requiresSynchronization = decision.requiresHostSynchronization,
                .usesWindowsMl = decision.usesWindowsMl,
                .reason = decision.reason,
            };
            plan.hostAssistRequired =
                plan.hostAssistRequired || slice.target == MixedDispatchTarget::kHostAssist;
            plan.cpuFallbackPresent =
                plan.cpuFallbackPresent || slice.target == MixedDispatchTarget::kCpuFallback;
            plan.slices.push_back(std::move(slice));
        }

        return plan;
    }

} // namespace us4::runtime::backends::windows_ml
