#include "us4/runtime/backends/windows_ml/layer_offloader.h"

namespace us4::runtime::backends::windows_ml
{

    namespace
    {
        LayerOffloadDecision MakeDecision(const LayerDescriptor& layer, LayerExecutionTarget target,
                                          const WindowsMlPartitionKind partitionKind,
                                          const bool usesWindowsMl, const bool requiresSync,
                                          std::string reason)
        {
            return LayerOffloadDecision{
                .layerName = layer.name,
                .target = target,
                .partitionKind = partitionKind,
                .usesWindowsMl = usesWindowsMl,
                .requiresHostSynchronization = requiresSync,
                .reason = std::move(reason),
            };
        }
    } // namespace

    std::string ToString(const LayerKind kind)
    {
        switch (kind)
        {
        case LayerKind::kEmbedding:
            return "embedding";
        case LayerKind::kDensePrefill:
            return "dense-prefill";
        case LayerKind::kDenseDecode:
            return "dense-decode";
        case LayerKind::kAttention:
            return "attention";
        case LayerKind::kKvCompression:
            return "kv-compression";
        case LayerKind::kMoERouter:
            return "moe-router";
        case LayerKind::kVisionProjector:
            return "vision-projector";
        }

        return "unknown";
    }

    std::string ToString(const LayerExecutionTarget target)
    {
        switch (target)
        {
        case LayerExecutionTarget::kNpu:
            return "npu";
        case LayerExecutionTarget::kGpu:
            return "gpu";
        case LayerExecutionTarget::kCpu:
            return "cpu";
        case LayerExecutionTarget::kHostAssist:
            return "host-assist";
        }

        return "unknown";
    }

    LayerOffloadDecision LayerOffloader::Decide(const WindowsMlExecutionPlan& plan,
                                                const LayerDescriptor& layer)
    {
        switch (layer.kind)
        {
        case LayerKind::kDensePrefill:
            if (plan.offloadPrefill)
            {
                return MakeDecision(layer, LayerExecutionTarget::kNpu,
                                    WindowsMlPartitionKind::kPrefillEncoder, true, false,
                                    "Dense prefill is eligible for Windows ML offload.");
            }
            return MakeDecision(layer, LayerExecutionTarget::kCpu,
                                WindowsMlPartitionKind::kCpuFallback, false, true,
                                "Prefill offload is disabled for the current plan.");

        case LayerKind::kDenseDecode:
            if (plan.offloadDecode)
            {
                return MakeDecision(layer, LayerExecutionTarget::kNpu,
                                    WindowsMlPartitionKind::kDecodeProjection, true, false,
                                    "Latency-sensitive dense decode stays on the NPU path.");
            }
            return MakeDecision(layer, LayerExecutionTarget::kCpu,
                                WindowsMlPartitionKind::kCpuFallback, false, true,
                                "Decode offload is disabled for the current plan.");

        case LayerKind::kKvCompression:
            if (plan.compressKvOnHost)
            {
                return MakeDecision(layer, LayerExecutionTarget::kHostAssist,
                                    WindowsMlPartitionKind::kKvCompression, false, true,
                                    "KV compression remains on the host for the current memory mode.");
            }
            return MakeDecision(layer, LayerExecutionTarget::kCpu,
                                WindowsMlPartitionKind::kCpuFallback, false, false,
                                "No dedicated KV compression path is required.");

        case LayerKind::kMoERouter:
            if (plan.routeMoEOnHost)
            {
                return MakeDecision(layer, LayerExecutionTarget::kHostAssist,
                                    WindowsMlPartitionKind::kHostAssist, false, true,
                                    "MoE routing remains on the host path.");
            }
            return MakeDecision(layer, LayerExecutionTarget::kCpu,
                                WindowsMlPartitionKind::kCpuFallback, false, false,
                                "MoE host routing is not needed for this plan.");

        case LayerKind::kVisionProjector:
            return MakeDecision(layer, LayerExecutionTarget::kHostAssist,
                                WindowsMlPartitionKind::kHostAssist, false, true,
                                "Vision pre-processing stays on the host path.");

        case LayerKind::kAttention:
            return MakeDecision(layer, LayerExecutionTarget::kCpu,
                                WindowsMlPartitionKind::kCpuFallback, false, true,
                                "Attention remains outside the Windows ML dense offload scope.");

        case LayerKind::kEmbedding:
            if (plan.optInSatisfied)
            {
                return MakeDecision(layer, LayerExecutionTarget::kNpu,
                                    WindowsMlPartitionKind::kGraphCompile, true, false,
                                    "Embedding projection follows the compiled dense graph.");
            }
            return MakeDecision(layer, LayerExecutionTarget::kCpu,
                                WindowsMlPartitionKind::kCpuFallback, false, true,
                                "Embedding stays on CPU until Windows ML opt-in is satisfied.");
        }

        return MakeDecision(layer, LayerExecutionTarget::kCpu,
                            WindowsMlPartitionKind::kCpuFallback, false, true,
                            "Layer fell back to the CPU path.");
    }

    std::vector<LayerOffloadDecision>
    LayerOffloader::BuildDispatchTable(const WindowsMlExecutionPlan& plan,
                                       const std::vector<LayerDescriptor>& layers)
    {
        std::vector<LayerOffloadDecision> dispatchTable;
        dispatchTable.reserve(layers.size());
        for (const auto& layer : layers)
        {
            dispatchTable.push_back(Decide(plan, layer));
        }
        return dispatchTable;
    }

} // namespace us4::runtime::backends::windows_ml
