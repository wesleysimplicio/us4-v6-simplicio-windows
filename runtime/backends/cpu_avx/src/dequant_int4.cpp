#include "us4/runtime/backends/cpu_avx/dequant_int4.h"

#include <algorithm>
#include <array>
#include <immintrin.h>
#include <stdexcept>

namespace us4::runtime::backends::cpu_avx
{
    namespace
    {

        std::size_t ComputeExpectedGroupCount(const std::size_t elementCount,
                                              const std::size_t groupSize)
        {
            if (groupSize == 0U)
            {
                throw std::invalid_argument("Group-wise INT4 dequant requires groupSize > 0.");
            }

            return (elementCount + groupSize - 1U) / groupSize;
        }

        std::int8_t DecodeNibble(const std::uint8_t nibble) noexcept
        {
            return nibble >= 8U ? static_cast<std::int8_t>(nibble) - 16 : static_cast<std::int8_t>(nibble);
        }

    } // namespace

    us4::core::Tensor DequantizeInt4GroupWise(const std::vector<std::uint8_t>& packedValues,
                                              const std::size_t rows, const std::size_t cols,
                                              const std::size_t groupSize,
                                              const std::vector<float>& scales)
    {
        const std::size_t elementCount = rows * cols;
        const std::size_t expectedPackedBytes = (elementCount + 1U) / 2U;
        if (packedValues.size() != expectedPackedBytes)
        {
            throw std::invalid_argument(
                "Group-wise INT4 dequant expects ceil(rows * cols / 2) packed bytes.");
        }

        const std::size_t expectedGroups = ComputeExpectedGroupCount(elementCount, groupSize);
        if (scales.size() != expectedGroups)
        {
            throw std::invalid_argument(
                "Group-wise INT4 dequant expects one scale per contiguous quantized group.");
        }

        us4::core::Tensor output({rows, cols}, us4::core::TensorDataType::kFloat32);
        float* outputData = output.Data();

        std::array<std::int8_t, 32U> unpacked{};
        for (std::size_t groupIndex = 0U; groupIndex < expectedGroups; ++groupIndex)
        {
            const std::size_t groupBegin = groupIndex * groupSize;
            const std::size_t groupEnd = std::min(groupBegin + groupSize, elementCount);
            const float scale = scales[groupIndex];

            std::size_t valueIndex = groupBegin;
            while (valueIndex + 32U <= groupEnd)
            {
                const std::size_t packedOffset = valueIndex / 2U;
                const __m128i packedBytes = _mm_loadu_si128(
                    reinterpret_cast<const __m128i*>(packedValues.data() +
                                                     static_cast<std::ptrdiff_t>(packedOffset)));
                const __m256i widened = _mm256_cvtepu8_epi16(packedBytes);
                const __m256i lowNibbles = _mm256_and_si256(widened, _mm256_set1_epi16(0x0F));
                const __m256i highNibbles =
                    _mm256_and_si256(_mm256_srli_epi16(widened, 4), _mm256_set1_epi16(0x0F));

                const __m256i signMaskLow =
                    _mm256_cmpgt_epi16(lowNibbles, _mm256_set1_epi16(7));
                const __m256i signMaskHigh =
                    _mm256_cmpgt_epi16(highNibbles, _mm256_set1_epi16(7));
                const __m256i signedLow =
                    _mm256_sub_epi16(lowNibbles, _mm256_and_si256(signMaskLow, _mm256_set1_epi16(16)));
                const __m256i signedHigh = _mm256_sub_epi16(
                    highNibbles, _mm256_and_si256(signMaskHigh, _mm256_set1_epi16(16)));

                alignas(32) std::int16_t lowValues[16];
                alignas(32) std::int16_t highValues[16];
                _mm256_store_si256(reinterpret_cast<__m256i*>(lowValues), signedLow);
                _mm256_store_si256(reinterpret_cast<__m256i*>(highValues), signedHigh);

                for (std::size_t lane = 0U; lane < 16U; ++lane)
                {
                    unpacked[lane * 2U] = static_cast<std::int8_t>(lowValues[lane]);
                    unpacked[(lane * 2U) + 1U] = static_cast<std::int8_t>(highValues[lane]);
                }

                std::size_t unpackIndex = 0U;
                for (; unpackIndex + 8U <= unpacked.size(); unpackIndex += 8U)
                {
                    const __m128i block = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(
                        unpacked.data() + static_cast<std::ptrdiff_t>(unpackIndex)));
                    const __m256i widenedBlock = _mm256_cvtepi8_epi32(block);
                    const __m256 floats = _mm256_cvtepi32_ps(widenedBlock);
                    const __m256 scaled = _mm256_mul_ps(floats, _mm256_set1_ps(scale));
                    _mm256_storeu_ps(outputData + valueIndex + unpackIndex, scaled);
                }

                valueIndex += 32U;
            }

            for (; valueIndex < groupEnd; ++valueIndex)
            {
                const std::size_t packedIndex = valueIndex / 2U;
                const bool highNibble = (valueIndex % 2U) == 1U;
                const std::uint8_t packed = packedValues[packedIndex];
                const std::uint8_t nibble =
                    highNibble ? static_cast<std::uint8_t>((packed >> 4) & 0x0F) : static_cast<std::uint8_t>(packed & 0x0F);
                outputData[valueIndex] = static_cast<float>(DecodeNibble(nibble)) * scale;
            }
        }

        return output;
    }

} // namespace us4::runtime::backends::cpu_avx
