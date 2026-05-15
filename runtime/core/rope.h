#pragma once

#include "runtime/core/tensor.h"

#include <cstddef>

namespace us4::core
{

    enum class RopeScalingMode
    {
        kLinear,
        kDynamic,
        kYarn,
    };

    struct RopeConfig
    {
        float baseTheta = 10000.0F;
        float scalingFactor = 1.0F;
        float yarnAttentionFactor = 1.0F;
        RopeScalingMode scalingMode = RopeScalingMode::kLinear;
    };

    [[nodiscard]] Tensor ApplyRoPE(const Tensor& input, std::size_t position, const RopeConfig& config);

} // namespace us4::core
