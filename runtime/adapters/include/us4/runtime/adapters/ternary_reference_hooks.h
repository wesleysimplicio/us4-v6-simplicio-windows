#pragma once

#include "us4/runtime/adapters/ternary_adapter_contracts.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace us4::runtime::adapters
{

    struct TernaryPackedWeights
    {
        TernaryExecutionHints hints;
        std::size_t rows = 0;
        std::size_t columns = 0;
        std::vector<std::int8_t> values;
    };

    [[nodiscard]] TernaryPackedWeights
    PackTernaryReferenceWeights(const std::vector<float>& weights, std::size_t rows,
                                std::size_t columns, TernaryExecutionHints hints);
    [[nodiscard]] std::vector<float>
    RunTernaryReferenceMatVec(const TernaryPackedWeights& packedWeights,
                              const std::vector<float>& activations);
    [[nodiscard]] std::size_t EstimateTernaryPackedBytes(const TernaryModelDescriptor& descriptor);

} // namespace us4::runtime::adapters
