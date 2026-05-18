#include "us4/runtime/backends/cpu_avx/bitnet_matmul.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <immintrin.h>
#include <stdexcept>

namespace us4::runtime::backends::cpu_avx
{
    namespace
    {

        struct alignas(32) BitMaskLookupRow
        {
            std::array<float, 8> lanes{};
        };

        [[nodiscard]] bool GetBit(const std::vector<std::uint8_t>& bytes, const std::size_t index)
        {
            return (bytes[index / 8U] & static_cast<std::uint8_t>(1U << (index % 8U))) != 0U;
        }

        [[nodiscard]] std::uint32_t PopCount8(const std::uint8_t value) noexcept
        {
#if defined(_MSC_VER)
            return static_cast<std::uint32_t>(__popcnt16(value));
#else
            return static_cast<std::uint32_t>(__builtin_popcount(value));
#endif
        }

        [[nodiscard]] const std::array<BitMaskLookupRow, 256>& BitMaskLookupTable() noexcept
        {
            static const std::array<BitMaskLookupRow, 256> kLookup = []() noexcept
            {
                std::array<BitMaskLookupRow, 256> table{};
                for (std::size_t mask = 0U; mask < table.size(); ++mask)
                {
                    for (std::size_t bit = 0U; bit < table[mask].lanes.size(); ++bit)
                    {
                        table[mask].lanes[bit] =
                            (mask & (static_cast<std::size_t>(1U) << bit)) != 0U ? 1.0F : 0.0F;
                    }
                }
                return table;
            }();

            return kLookup;
        }

        [[nodiscard]] float HorizontalAdd(const __m256 value) noexcept
        {
            alignas(32) float lanes[8];
            _mm256_store_ps(lanes, value);

            float accumulator = 0.0F;
            for (const float lane : lanes)
            {
                accumulator += lane;
            }
            return accumulator;
        }

    } // namespace

    float DotPackedBitNetAvx2(const std::vector<float>& activations, const PackedBitNetRow& packed)
    {
        if (activations.size() != packed.elementCount)
        {
            throw std::invalid_argument("DotPackedBitNetAvx2 activation length mismatch.");
        }

        if (packed.positiveBits.size() != packed.negativeBits.size())
        {
            throw std::invalid_argument(
                "DotPackedBitNetAvx2 packed bit lanes must have matching sizes.");
        }

        const auto& lookupTable = BitMaskLookupTable();
        __m256 accumulator = _mm256_setzero_ps();
        std::size_t activationIndex = 0U;
        const std::size_t fullBytes = packed.elementCount / 8U;

        for (std::size_t byteIndex = 0U; byteIndex < fullBytes; ++byteIndex)
        {
            const std::uint8_t positiveMask = packed.positiveBits[byteIndex];
            const std::uint8_t negativeMask = packed.negativeBits[byteIndex];

            if (PopCount8(static_cast<std::uint8_t>(positiveMask | negativeMask)) == 0U)
            {
                activationIndex += 8U;
                continue;
            }

            const __m256 activationBlock = _mm256_loadu_ps(activations.data() + activationIndex);
            const __m256 positiveLut = _mm256_load_ps(lookupTable[positiveMask].lanes.data());
            const __m256 negativeLut = _mm256_load_ps(lookupTable[negativeMask].lanes.data());
            const __m256 signVector = _mm256_sub_ps(positiveLut, negativeLut);
            accumulator = _mm256_add_ps(accumulator, _mm256_mul_ps(activationBlock, signVector));
            activationIndex += 8U;
        }

        float scalarAccumulator = HorizontalAdd(accumulator);
        if (activationIndex < packed.elementCount)
        {
            for (; activationIndex < packed.elementCount; ++activationIndex)
            {
                if (GetBit(packed.positiveBits, activationIndex))
                {
                    scalarAccumulator += activations[activationIndex];
                }
                else if (GetBit(packed.negativeBits, activationIndex))
                {
                    scalarAccumulator -= activations[activationIndex];
                }
            }
        }

        return scalarAccumulator;
    }

} // namespace us4::runtime::backends::cpu_avx
