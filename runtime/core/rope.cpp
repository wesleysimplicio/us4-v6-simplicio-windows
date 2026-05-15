#include "runtime/core/rope.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace us4::core
{
    namespace
    {
        float EffectiveScale(const RopeConfig& config, const std::size_t position)
        {
            switch (config.scalingMode)
            {
            case RopeScalingMode::kLinear:
                return config.scalingFactor;
            case RopeScalingMode::kDynamic:
                return config.scalingFactor + static_cast<float>(position) * 0.01F;
            case RopeScalingMode::kYarn:
                return config.scalingFactor * config.yarnAttentionFactor;
            }

            return config.scalingFactor;
        }
    } // namespace

    Tensor ApplyRoPE(const Tensor& input, const std::size_t position, const RopeConfig& config)
    {
        if (input.Rank() != 2U)
        {
            throw std::invalid_argument("RoPE currently expects a rank-2 [tokens, head_dim] tensor.");
        }

        const std::size_t tokenCount = input.Dim(0);
        const std::size_t headDim = input.Dim(1);
        if (headDim % 2U != 0U)
        {
            throw std::invalid_argument("RoPE requires an even head dimension.");
        }

        Tensor output(input.Shape(), input.DataType());
        const float scale = EffectiveScale(config, position);

        for (std::size_t token = 0; token < tokenCount; ++token)
        {
            for (std::size_t channel = 0; channel < headDim; channel += 2U)
            {
                const float indexFactor = static_cast<float>(channel / 2U) /
                                          static_cast<float>(std::max<std::size_t>(1U, headDim / 2U));
                const float theta = std::pow(config.baseTheta, -indexFactor);
                const float angle = static_cast<float>(position + token) * theta / scale;
                const float cosAngle = std::cos(angle);
                const float sinAngle = std::sin(angle);

                const float even = input.At({token, channel});
                const float odd = input.At({token, channel + 1U});

                output.At({token, channel}) = even * cosAngle - odd * sinAngle;
                output.At({token, channel + 1U}) = even * sinAngle + odd * cosAngle;
            }
        }

        return output;
    }

} // namespace us4::core
