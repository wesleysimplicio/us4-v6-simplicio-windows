#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace us4::runtime::backends::cpu_avx
{

    struct PackedBitNetRow
    {
        std::vector<std::uint8_t> positiveBits;
        std::vector<std::uint8_t> negativeBits;
        std::size_t elementCount = 0;
    };

    [[nodiscard]] PackedBitNetRow PackBitNetRow(const std::vector<float>& values,
                                                float ternaryThreshold = 0.25F);
    [[nodiscard]] std::vector<int8_t> UnpackBitNetRow(const PackedBitNetRow& packed);
    [[nodiscard]] float DotPackedBitNet(const std::vector<float>& activations,
                                        const PackedBitNetRow& packed);

    [[nodiscard]] std::uint8_t PackTernaryChunk(const std::array<int8_t, 4>& values) noexcept;
    [[nodiscard]] std::array<int8_t, 4> DecodeTernaryChunk(std::uint8_t packed) noexcept;
    [[nodiscard]] std::array<std::array<int8_t, 4>, 256> BuildTernaryLookupTable() noexcept;
    [[nodiscard]] float DotPackedTernary(const std::vector<float>& activations,
                                         const std::vector<std::uint8_t>& packedWeights);

} // namespace us4::runtime::backends::cpu_avx
