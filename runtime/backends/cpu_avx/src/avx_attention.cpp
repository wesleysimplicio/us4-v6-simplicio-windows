#include "us4/runtime/backends/cpu_avx/avx_attention.h"

#include <algorithm>
#include <cmath>
#include <immintrin.h>
#include <limits>
#include <stdexcept>
#include <vector>

namespace us4::runtime::backends::cpu_avx
{
    namespace
    {

        std::vector<std::size_t> InferAttentionPrefix(const us4::core::Tensor& tensor)
        {
            std::vector<std::size_t> prefix;
            const std::size_t rank = tensor.Rank();
            for (std::size_t i = 0; i + 2 < rank; ++i)
            {
                prefix.push_back(tensor.Dim(i));
            }
            return prefix;
        }

        std::size_t ComputePrefixGroupCount(const us4::core::Tensor& tensor)
        {
            const std::vector<std::size_t> prefix = InferAttentionPrefix(tensor);
            if (prefix.empty())
            {
                return 1;
            }
            return us4::core::Tensor::ComputeElementCount(prefix);
        }

        std::size_t RowMajorOffset(std::size_t groupIndex, std::size_t rowIndex,
                                   std::size_t colIndex, std::size_t rowCount, std::size_t colCount)
        {
            return ((groupIndex * rowCount) + rowIndex) * colCount + colIndex;
        }

        us4::core::Tensor ConcatAttentionCache(const us4::core::Tensor* cached,
                                               const us4::core::Tensor& current)
        {
            if (cached == nullptr)
            {
                return current;
            }

            std::vector<std::size_t> outputShape = current.Shape();
            outputShape[outputShape.size() - 2] += cached->Dim(cached->Rank() - 2);

            us4::core::Tensor output(outputShape, current.DataType());
            output.Fill(0.0F);

            const std::size_t rank = current.Rank();
            const std::size_t groups = ComputePrefixGroupCount(current);
            const std::size_t cachedRows = cached->Dim(rank - 2);
            const std::size_t currentRows = current.Dim(rank - 2);
            const std::size_t cols = current.Dim(rank - 1);

            for (std::size_t group = 0; group < groups; ++group)
            {
                for (std::size_t row = 0; row < cachedRows; ++row)
                {
                    for (std::size_t col = 0; col < cols; ++col)
                    {
                        output[RowMajorOffset(group, row, col, cachedRows + currentRows, cols)] =
                            (*cached)[RowMajorOffset(group, row, col, cachedRows, cols)];
                    }
                }
                for (std::size_t row = 0; row < currentRows; ++row)
                {
                    for (std::size_t col = 0; col < cols; ++col)
                    {
                        output[RowMajorOffset(group, cachedRows + row, col,
                                              cachedRows + currentRows, cols)] =
                            current[RowMajorOffset(group, row, col, currentRows, cols)];
                    }
                }
            }

            return output;
        }

        float DotProductAvx2(const float* left, const float* right, const std::size_t count) noexcept
        {
            __m256 acc = _mm256_setzero_ps();
            std::size_t index = 0U;
            for (; index + 8U <= count; index += 8U)
            {
                const __m256 lhs = _mm256_loadu_ps(left + index);
                const __m256 rhs = _mm256_loadu_ps(right + index);
                acc = _mm256_add_ps(acc, _mm256_mul_ps(lhs, rhs));
            }

            alignas(32) float partial[8];
            _mm256_store_ps(partial, acc);
            float sum = 0.0F;
            for (const float value : partial)
            {
                sum += value;
            }
            for (; index < count; ++index)
            {
                sum += left[index] * right[index];
            }

            return sum;
        }

        void ScaleAccumulator(std::vector<float>& values, const float factor) noexcept
        {
            std::size_t index = 0U;
            const __m256 scale = _mm256_set1_ps(factor);
            for (; index + 8U <= values.size(); index += 8U)
            {
                __m256 block = _mm256_loadu_ps(values.data() + index);
                block = _mm256_mul_ps(block, scale);
                _mm256_storeu_ps(values.data() + index, block);
            }
            for (; index < values.size(); ++index)
            {
                values[index] *= factor;
            }
        }

        void AddWeightedValue(std::vector<float>& accumulator, const float* valueRow,
                              const std::size_t width, const float weight) noexcept
        {
            std::size_t index = 0U;
            const __m256 weightBlock = _mm256_set1_ps(weight);
            for (; index + 8U <= width; index += 8U)
            {
                __m256 acc = _mm256_loadu_ps(accumulator.data() + index);
                const __m256 value = _mm256_loadu_ps(valueRow + index);
                acc = _mm256_add_ps(acc, _mm256_mul_ps(value, weightBlock));
                _mm256_storeu_ps(accumulator.data() + index, acc);
            }
            for (; index < width; ++index)
            {
                accumulator[index] += valueRow[index] * weight;
            }
        }

    } // namespace

    us4::core::Tensor AvxAttention(const us4::core::Tensor& query, const us4::core::Tensor& key,
                                   const us4::core::Tensor& value, const AttentionOptions& options)
    {
        const AttentionValidation validation = ValidateScalarAttention(query, key, value, options);
        if (!validation.ok)
        {
            throw std::invalid_argument(validation.message);
        }

        const us4::core::Tensor effectiveKey = ConcatAttentionCache(options.cachedKey, key);
        const us4::core::Tensor effectiveValue = ConcatAttentionCache(options.cachedValue, value);

        const std::size_t rank = query.Rank();
        const std::size_t queryTokens = query.Dim(rank - 2);
        const std::size_t keyTokens = effectiveKey.Dim(rank - 2);
        const std::size_t hiddenSize = query.Dim(rank - 1);
        const std::size_t valueHidden = effectiveValue.Dim(rank - 1);
        const std::size_t groups = ComputePrefixGroupCount(query);
        const std::size_t cachedTokens = options.cachedKey != nullptr
                                             ? options.cachedKey->Dim(options.cachedKey->Rank() - 2)
                                             : 0U;

        const float effectiveScale = options.scale > 0.0F
                                         ? options.scale
                                         : (1.0F / std::sqrt(static_cast<float>(hiddenSize)));

        us4::core::Tensor output(query.Shape());
        if (valueHidden != hiddenSize)
        {
            std::vector<std::size_t> outputShape = query.Shape();
            outputShape.back() = valueHidden;
            output.Resize(outputShape);
        }
        output.Fill(0.0F);

        std::vector<float> accumulator(valueHidden, 0.0F);

        for (std::size_t group = 0; group < groups; ++group)
        {
            for (std::size_t q = 0; q < queryTokens; ++q)
            {
                std::fill(accumulator.begin(), accumulator.end(), 0.0F);
                const std::size_t visibleTokens =
                    options.causalMask ? std::min<std::size_t>(keyTokens, cachedTokens + q + 1U)
                                       : keyTokens;

                float runningMax = -std::numeric_limits<float>::infinity();
                float runningNormalizer = 0.0F;
                const float* queryRow =
                    query.Data() + RowMajorOffset(group, q, 0U, queryTokens, hiddenSize);

                for (std::size_t kIndex = 0; kIndex < visibleTokens; ++kIndex)
                {
                    const float* keyRow = effectiveKey.Data() +
                                          RowMajorOffset(group, kIndex, 0U, keyTokens, hiddenSize);
                    const float* valueRow = effectiveValue.Data() +
                                            RowMajorOffset(group, kIndex, 0U, keyTokens, valueHidden);

                    const float score = DotProductAvx2(queryRow, keyRow, hiddenSize) * effectiveScale;
                    const float nextMax = std::max(runningMax, score);
                    const float previousFactor =
                        std::isfinite(runningMax) ? std::exp(runningMax - nextMax) : 0.0F;
                    const float currentWeight = std::exp(score - nextMax);

                    if (previousFactor != 1.0F)
                    {
                        ScaleAccumulator(accumulator, previousFactor);
                    }
                    AddWeightedValue(accumulator, valueRow, valueHidden, currentWeight);

                    runningNormalizer = (runningNormalizer * previousFactor) + currentWeight;
                    runningMax = nextMax;
                }

                const float inverseNormalizer =
                    runningNormalizer > 0.0F ? (1.0F / runningNormalizer) : 0.0F;
                ScaleAccumulator(accumulator, inverseNormalizer);

                for (std::size_t valueIndex = 0; valueIndex < valueHidden; ++valueIndex)
                {
                    output[RowMajorOffset(group, q, valueIndex, queryTokens, valueHidden)] =
                        accumulator[valueIndex];
                }
            }
        }

        return output;
    }

} // namespace us4::runtime::backends::cpu_avx
