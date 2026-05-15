#pragma once

#include "us4/runtime/adapters/bitnet_adapter_contracts.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace us4::runtime::adapters
{

    struct BitNetPackedWeights
    {
        BitNetExecutionHints hints;
        std::size_t rows = 0;
        std::size_t columns = 0;
        std::vector<std::int8_t> values;
    };

    [[nodiscard]] BitNetPackedWeights PackBitNetReferenceWeights(const std::vector<float>& weights,
                                                                 std::size_t rows,
                                                                 std::size_t columns,
                                                                 BitNetExecutionHints hints);
    [[nodiscard]] std::vector<float>
    RunBitNetReferenceMatVec(const BitNetPackedWeights& packedWeights,
                             const std::vector<float>& activations);
    [[nodiscard]] std::size_t EstimateBitNetPackedBytes(const BitNetModelDescriptor& descriptor);

} // namespace us4::runtime::adapters
