#include "us4/runtime/backends/cpu_avx/bitnet_reference.h"

#include <algorithm>
#include <stdexcept>

namespace us4::runtime::backends::cpu_avx
{
    namespace
    {

        void SetBit(std::vector<std::uint8_t>& bytes, const std::size_t index)
        {
            bytes[index / 8U] |= static_cast<std::uint8_t>(1U << (index % 8U));
        }

        [[nodiscard]] bool GetBit(const std::vector<std::uint8_t>& bytes, const std::size_t index)
        {
            return (bytes[index / 8U] & static_cast<std::uint8_t>(1U << (index % 8U))) != 0U;
        }

        [[nodiscard]] int8_t ClampTernary(const int8_t value) noexcept
        {
            if (value > 0)
            {
                return 1;
            }
            if (value < 0)
            {
                return -1;
            }
            return 0;
        }

        [[nodiscard]] std::uint8_t EncodeTernaryDigit(const int8_t value) noexcept
        {
            switch (ClampTernary(value))
            {
            case -1:
                return 0U;
            case 0:
                return 1U;
            case 1:
            default:
                return 2U;
            }
        }

        [[nodiscard]] int8_t DecodeTernaryDigit(const std::uint8_t value) noexcept
        {
            switch (value)
            {
            case 0U:
                return -1;
            case 1U:
                return 0;
            case 2U:
                return 1;
            default:
                return 0;
            }
        }

    } // namespace

    PackedBitNetRow PackBitNetRow(const std::vector<float>& values, const float ternaryThreshold)
    {
        PackedBitNetRow packed;
        packed.elementCount = values.size();
        const std::size_t byteCount = (values.size() + 7U) / 8U;
        packed.positiveBits.assign(byteCount, 0U);
        packed.negativeBits.assign(byteCount, 0U);

        for (std::size_t index = 0; index < values.size(); ++index)
        {
            if (values[index] >= ternaryThreshold)
            {
                SetBit(packed.positiveBits, index);
            }
            else if (values[index] <= -ternaryThreshold)
            {
                SetBit(packed.negativeBits, index);
            }
        }

        return packed;
    }

    std::vector<int8_t> UnpackBitNetRow(const PackedBitNetRow& packed)
    {
        std::vector<int8_t> values(packed.elementCount, 0);
        for (std::size_t index = 0; index < packed.elementCount; ++index)
        {
            const bool positive = GetBit(packed.positiveBits, index);
            const bool negative = GetBit(packed.negativeBits, index);
            values[index] = positive ? 1 : (negative ? -1 : 0);
        }
        return values;
    }

    float DotPackedBitNet(const std::vector<float>& activations, const PackedBitNetRow& packed)
    {
        if (activations.size() != packed.elementCount)
        {
            throw std::invalid_argument("DotPackedBitNet activation length mismatch.");
        }

        float acc = 0.0F;
        for (std::size_t index = 0; index < packed.elementCount; ++index)
        {
            if (GetBit(packed.positiveBits, index))
            {
                acc += activations[index];
            }
            else if (GetBit(packed.negativeBits, index))
            {
                acc -= activations[index];
            }
        }

        return acc;
    }

    std::uint8_t PackTernaryChunk(const std::array<int8_t, 4>& values) noexcept
    {
        std::uint8_t packed = 0U;
        std::uint8_t radix = 1U;
        for (const int8_t value : values)
        {
            packed = static_cast<std::uint8_t>(packed + (EncodeTernaryDigit(value) * radix));
            radix = static_cast<std::uint8_t>(radix * 3U);
        }
        return packed;
    }

    std::array<int8_t, 4> DecodeTernaryChunk(const std::uint8_t packed) noexcept
    {
        std::array<int8_t, 4> values{};
        std::uint8_t residue = packed;
        for (std::size_t index = 0; index < values.size(); ++index)
        {
            values[index] = DecodeTernaryDigit(static_cast<std::uint8_t>(residue % 3U));
            residue = static_cast<std::uint8_t>(residue / 3U);
        }
        return values;
    }

    std::array<std::array<int8_t, 4>, 256> BuildTernaryLookupTable() noexcept
    {
        std::array<std::array<int8_t, 4>, 256> table{};
        for (std::size_t index = 0; index < table.size(); ++index)
        {
            table[index] = DecodeTernaryChunk(static_cast<std::uint8_t>(index));
        }
        return table;
    }

    float DotPackedTernary(const std::vector<float>& activations,
                           const std::vector<std::uint8_t>& packedWeights)
    {
        const auto lut = BuildTernaryLookupTable();
        float acc = 0.0F;
        std::size_t activationIndex = 0U;

        for (const std::uint8_t packed : packedWeights)
        {
            const auto& decoded = lut[packed];
            for (const int8_t weight : decoded)
            {
                if (activationIndex >= activations.size())
                {
                    return acc;
                }

                acc += static_cast<float>(weight) * activations[activationIndex];
                ++activationIndex;
            }
        }

        return acc;
    }

} // namespace us4::runtime::backends::cpu_avx
