#include "us4/runtime/backends/windows_ml/mixed_dispatch_planner.h"

namespace us4::runtime::backends::windows_ml
{

    namespace
    {
        MixedDispatchTarget ResolveDemotionTarget(const vulkan::VulkanExecutionPlan& gpuPlan)
        {
            return gpuPlan.steps.empty() ? MixedDispatchTarget::kCpuFallback
                                         : MixedDispatchTarget::kGpuPrimary;
        }

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
                                                  const std::vector<LayerDescriptor>& layers,
                                                  const PowerThermalSnapshot& snapshot,
                                                  const WinMlSessionArtifact* artifact)
    {
        MixedDispatchPlan plan{};
        plan.gpuPrimaryActive = !gpuPlan.steps.empty();
        plan.policy = PowerThermalMonitor::SelectPolicy(snapshot);
        const auto offloadDecisions = LayerOffloader::BuildDispatchTable(npuPlan, layers);
        const bool compileFallbackActive =
            artifact != nullptr && artifact->compileTarget == WinMlCompileTarget::kCpuFallback;

        plan.slices.reserve(offloadDecisions.size());
        for (std::size_t index = 0; index < offloadDecisions.size(); ++index)
        {
            const auto& decision = offloadDecisions[index];
            const auto& layer = layers[index];
            const MixedDispatchTarget originalTarget = ResolveTarget(decision);
            MixedDispatchTarget target = originalTarget;
            bool changedByPolicy = false;
            std::string reason = decision.reason;

            if (originalTarget == MixedDispatchTarget::kNpuDense && compileFallbackActive)
            {
                target = ResolveDemotionTarget(gpuPlan);
                reason +=
                    " Compiled Windows ML session armed CPU fallback because NPU execution is "
                    "unavailable for this run.";
            }
            else if (originalTarget == MixedDispatchTarget::kNpuDense)
            {
                if (plan.policy == PowerDispatchPolicy::kPreferEfficiency &&
                    (layer.latencySensitive || layer.kind == LayerKind::kDenseDecode))
                {
                    target = ResolveDemotionTarget(gpuPlan);
                    changedByPolicy = true;
                    reason +=
                        " Power policy demoted latency-sensitive dense decode off the NPU path.";
                }
                else if (plan.policy == PowerDispatchPolicy::kThermalThrottle)
                {
                    target = ResolveDemotionTarget(gpuPlan);
                    changedByPolicy = true;
                    reason +=
                        " Thermal throttle policy demoted dense NPU work to the fallback path.";
                }
                else if (plan.policy == PowerDispatchPolicy::kCriticalFallback)
                {
                    target = ResolveDemotionTarget(gpuPlan);
                    changedByPolicy = true;
                    reason +=
                        " Critical thermal policy disabled dense NPU execution for this session.";
                }
            }

            MixedDispatchSlice slice{
                .layerName = decision.layerName,
                .target = target,
                .requiresSynchronization = decision.requiresHostSynchronization,
                .usesWindowsMl = decision.usesWindowsMl,
                .reason = std::move(reason),
            };
            if (slice.target != originalTarget && changedByPolicy)
            {
                plan.degradedByPolicy = true;
                ++plan.npuDemotionCount;
            }
            plan.npuDenseActive =
                plan.npuDenseActive || slice.target == MixedDispatchTarget::kNpuDense;
            plan.hostAssistRequired =
                plan.hostAssistRequired || slice.target == MixedDispatchTarget::kHostAssist;
            plan.cpuFallbackPresent =
                plan.cpuFallbackPresent || slice.target == MixedDispatchTarget::kCpuFallback;
            plan.slices.push_back(std::move(slice));
        }

        return plan;
    }

} // namespace us4::runtime::backends::windows_ml
