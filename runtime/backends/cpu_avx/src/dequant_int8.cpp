#include "us4/runtime/backends/cpu_avx/dequant_int8.h"

#include <algorithm>
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
                throw std::invalid_argument("Group-wise INT8 dequant requires groupSize > 0.");
            }

            return (elementCount + groupSize - 1U) / groupSize;
        }

    } // namespace

    us4::core::Tensor DequantizeInt8GroupWise(const std::vector<std::int8_t>& values,
                                              const std::size_t rows, const std::size_t cols,
                                              const std::size_t groupSize,
                                              const std::vector<float>& scales)
    {
        const std::size_t elementCount = rows * cols;
        if (values.size() != elementCount)
        {
            throw std::invalid_argument(
                "Group-wise INT8 dequant expects rows * cols quantized values.");
        }

        const std::size_t expectedGroups = ComputeExpectedGroupCount(elementCount, groupSize);
        if (scales.size() != expectedGroups)
        {
            throw std::invalid_argument(
                "Group-wise INT8 dequant expects one scale per contiguous quantized group.");
        }

        us4::core::Tensor output({rows, cols}, us4::core::TensorDataType::kFloat32);
        float* outputData = output.Data();

        for (std::size_t groupIndex = 0U; groupIndex < expectedGroups; ++groupIndex)
        {
            const std::size_t groupBegin = groupIndex * groupSize;
            const std::size_t groupEnd = std::min(groupBegin + groupSize, elementCount);
            const float scale = scales[groupIndex];

            std::size_t index = groupBegin;
            for (; index + 8U <= groupEnd; index += 8U)
            {
                const __m128i packed = _mm_loadl_epi64(
                    reinterpret_cast<const __m128i*>(values.data() + static_cast<std::ptrdiff_t>(index)));
                const __m256i widened = _mm256_cvtepi8_epi32(packed);
                const __m256 floats = _mm256_cvtepi32_ps(widened);
                const __m256 scaled = _mm256_mul_ps(floats, _mm256_set1_ps(scale));
                _mm256_storeu_ps(outputData + index, scaled);
            }

            for (; index < groupEnd; ++index)
            {
                outputData[index] = static_cast<float>(values[index]) * scale;
            }
        }

        return output;
    }

} // namespace us4::runtime::backends::cpu_avx
