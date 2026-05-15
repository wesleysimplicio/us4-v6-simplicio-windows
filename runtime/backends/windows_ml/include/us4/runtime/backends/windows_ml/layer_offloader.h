#pragma once

#include "us4/runtime/backends/windows_ml/windows_ml_execution_plan.h"

#include <string>
#include <string_view>
#include <vector>

namespace us4::runtime::backends::windows_ml
{

    enum class LayerKind
    {
        kEmbedding,
        kDensePrefill,
        kDenseDecode,
        kAttention,
        kKvCompression,
        kMoERouter,
        kVisionProjector,
    };

    enum class LayerExecutionTarget
    {
        kNpu,
        kGpu,
        kCpu,
        kHostAssist,
    };

    struct LayerDescriptor
    {
        std::string name;
        LayerKind kind = LayerKind::kDensePrefill;
        std::uint32_t batchHint = 1;
        std::uint32_t contextHint = 0;
        bool requiresStaticShapes = false;
        bool latencySensitive = false;
    };

    struct LayerOffloadDecision
    {
        std::string layerName;
        LayerExecutionTarget target = LayerExecutionTarget::kCpu;
        WindowsMlPartitionKind partitionKind = WindowsMlPartitionKind::kCpuFallback;
        bool usesWindowsMl = false;
        bool requiresHostSynchronization = false;
        std::string reason;
    };

    [[nodiscard]] std::string ToString(LayerKind kind);
    [[nodiscard]] std::string ToString(LayerExecutionTarget target);

    class LayerOffloader
    {
      public:
        [[nodiscard]] static LayerOffloadDecision
        Decide(const WindowsMlExecutionPlan& plan, const LayerDescriptor& layer);

        [[nodiscard]] static std::vector<LayerOffloadDecision>
        BuildDispatchTable(const WindowsMlExecutionPlan& plan,
                           const std::vector<LayerDescriptor>& layers);
    };

} // namespace us4::runtime::backends::windows_ml
