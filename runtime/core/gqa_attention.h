#pragma once

#include "runtime/core/tensor.h"

#include <cstddef>
#include <vector>

namespace us4::core
{

    struct GqaConfig
    {
        std::size_t queryHeads = 0;
        std::size_t kvHeads = 0;
        std::size_t sequenceLength = 0;
        std::size_t headDim = 0;
    };

    [[nodiscard]] std::vector<std::size_t> BuildGqaHeadMap(const GqaConfig& config);
    [[nodiscard]] Tensor ExpandGroupedHeads(const Tensor& kvTensor, const GqaConfig& config);

} // namespace us4::core
