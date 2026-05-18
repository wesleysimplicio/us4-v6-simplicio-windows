#include "us4/runtime/backends/cpu_avx/dequant_int4.h"
#include "us4/runtime/backends/cpu_avx/dequant_int8.h"

#include <cstdint>
#include <gtest/gtest.h>
#include <stdexcept>
#include <vector>

namespace us4::runtime::tests
{
    namespace
    {

        us4::core::Tensor ReferenceInt8(const std::vector<std::int8_t>& values,
                                        const std::size_t rows, const std::size_t cols,
                                        const std::size_t groupSize,
                                        const std::vector<float>& scales)
        {
            us4::core::Tensor output({rows, cols});
            for (std::size_t index = 0U; index < values.size(); ++index)
            {
                const std::size_t groupIndex = index / groupSize;
                output[index] = static_cast<float>(values[index]) * scales[groupIndex];
            }
            return output;
        }

        std::int8_t DecodeNibble(const std::uint8_t nibble) noexcept
        {
            return nibble >= 8U ? static_cast<std::int8_t>(nibble) - 16 : static_cast<std::int8_t>(nibble);
        }

        us4::core::Tensor ReferenceInt4(const std::vector<std::uint8_t>& packedValues,
                                        const std::size_t rows, const std::size_t cols,
                                        const std::size_t groupSize,
                                        const std::vector<float>& scales)
        {
            const std::size_t elementCount = rows * cols;
            us4::core::Tensor output({rows, cols});
            for (std::size_t index = 0U; index < elementCount; ++index)
            {
                const std::size_t packedIndex = index / 2U;
                const bool highNibble = (index % 2U) == 1U;
                const std::uint8_t nibble = highNibble
                                                ? static_cast<std::uint8_t>((packedValues[packedIndex] >> 4) & 0x0F)
                                                : static_cast<std::uint8_t>(packedValues[packedIndex] & 0x0F);
                output[index] =
                    static_cast<float>(DecodeNibble(nibble)) * scales[index / groupSize];
            }
            return output;
        }

    } // namespace

    TEST(CpuDequantTest, Int8GroupWiseMatchesScalarReference)
    {
        const std::vector<std::int8_t> values = {
            -12, -3, 0, 5, 14, -8, 7, 2, -1, 6, -7, 11,
        };
        const std::vector<float> scales = {0.25F, 0.5F, 1.25F};
        const auto reference = ReferenceInt8(values, 3U, 4U, 4U, scales);
        const auto dequantized =
            us4::runtime::backends::cpu_avx::DequantizeInt8GroupWise(values, 3U, 4U, 4U, scales);

        ASSERT_EQ(reference.ElementCount(), dequantized.ElementCount());
        for (std::size_t index = 0U; index < reference.ElementCount(); ++index)
        {
            EXPECT_NEAR(reference[index], dequantized[index], 1e-5F);
        }
    }

    TEST(CpuDequantTest, Int4GroupWiseMatchesScalarReference)
    {
        const std::vector<std::uint8_t> packedValues = {
            0x8F, 0x17, 0xC2, 0x54, 0x9A, 0x30,
        };
        const std::vector<float> scales = {0.25F, 0.5F, 1.25F};
        const auto reference = ReferenceInt4(packedValues, 3U, 4U, 4U, scales);
        const auto dequantized =
            us4::runtime::backends::cpu_avx::DequantizeInt4GroupWise(packedValues, 3U, 4U, 4U, scales);

        ASSERT_EQ(reference.ElementCount(), dequantized.ElementCount());
        for (std::size_t index = 0U; index < reference.ElementCount(); ++index)
        {
            EXPECT_NEAR(reference[index], dequantized[index], 1e-5F);
        }
    }

    TEST(CpuDequantTest, Int4RejectsMismatchedScaleCount)
    {
        const std::vector<std::uint8_t> packedValues = {0x12, 0x34, 0x56, 0x78};
        const std::vector<float> scales = {0.25F};

        EXPECT_THROW(
            static_cast<void>(us4::runtime::backends::cpu_avx::DequantizeInt4GroupWise(
                packedValues, 2U, 4U, 2U, scales)),
            std::invalid_argument);
    }
} // namespace us4::runtime::tests
