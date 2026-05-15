#include "us4/runtime/backends/cpu_avx/scalar_attention.h"

#include <cmath>
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

    } // namespace

    AttentionValidation ValidateScalarAttention(const us4::core::Tensor& query,
                                                const us4::core::Tensor& key,
                                                const us4::core::Tensor& value)
    {
        return ValidateScalarAttention(query, key, value, AttentionOptions{});
    }

    AttentionValidation ValidateScalarAttention(const us4::core::Tensor& query,
                                                const us4::core::Tensor& key,
                                                const us4::core::Tensor& value,
                                                const AttentionOptions& options)
    {
        if (query.Rank() < 2 || key.Rank() < 2 || value.Rank() < 2)
        {
            return {false, "ScalarAttention expects rank-2 or higher tensors with trailing "
                           "[sequence, hidden] dimensions."};
        }

        if (query.Rank() != key.Rank() || key.Rank() != value.Rank())
        {
            return {false, "Query, key, and value must have the same rank."};
        }

        if (query.Dim(query.Rank() - 1) != key.Dim(key.Rank() - 1))
        {
            return {false, "Query and key head dimensions must match."};
        }

        if (key.Dim(key.Rank() - 2) != value.Dim(value.Rank() - 2))
        {
            return {false, "Key and value sequence dimensions must match."};
        }

        for (std::size_t axis = 0; axis + 2 < query.Rank(); ++axis)
        {
            if (query.Dim(axis) != key.Dim(axis) || key.Dim(axis) != value.Dim(axis))
            {
                return {false, "Query, key, and value leading batch/head dimensions must match."};
            }
        }

        if (options.cachedKey != nullptr || options.cachedValue != nullptr)
        {
            if (options.cachedKey == nullptr || options.cachedValue == nullptr)
            {
                return {false, "Cached key/value tensors must be provided together."};
            }

            if (options.cachedKey->Rank() != key.Rank() ||
                options.cachedValue->Rank() != value.Rank())
            {
                return {false, "Cached key/value tensors must match the runtime attention rank."};
            }

            if (options.cachedKey->Dim(options.cachedKey->Rank() - 1) != key.Dim(key.Rank() - 1))
            {
                return {false,
                        "Cached key head dimension must match the current key head dimension."};
            }

            if (options.cachedValue->Dim(options.cachedValue->Rank() - 1) !=
                value.Dim(value.Rank() - 1))
            {
                return {
                    false,
                    "Cached value hidden dimension must match the current value hidden dimension.",
                };
            }
        }

        return {true, {}};
    }

    us4::core::Tensor ScalarAttention(const us4::core::Tensor& query, const us4::core::Tensor& key,
                                      const us4::core::Tensor& value,
                                      const AttentionOptions& options)
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

        std::vector<float> logits(keyTokens, 0.0F);
        std::vector<float> weights(keyTokens, 0.0F);

        for (std::size_t group = 0; group < groups; ++group)
        {
            for (std::size_t q = 0; q < queryTokens; ++q)
            {
                const std::size_t visibleTokens =
                    options.causalMask ? std::min<std::size_t>(keyTokens, cachedTokens + q + 1U)
                                       : keyTokens;
                float maxLogit = -std::numeric_limits<float>::infinity();
                for (std::size_t kIndex = 0; kIndex < visibleTokens; ++kIndex)
                {
                    float dot = 0.0F;
                    for (std::size_t hidden = 0; hidden < hiddenSize; ++hidden)
                    {
                        const std::size_t qOffset =
                            RowMajorOffset(group, q, hidden, queryTokens, hiddenSize);
                        const std::size_t kOffset =
                            RowMajorOffset(group, kIndex, hidden, keyTokens, hiddenSize);
                        dot += query[qOffset] * effectiveKey[kOffset];
                    }

                    logits[kIndex] = dot * effectiveScale;
                    if (logits[kIndex] > maxLogit)
                    {
                        maxLogit = logits[kIndex];
                    }
                }

                float normalizer = 0.0F;
                for (std::size_t kIndex = 0; kIndex < visibleTokens; ++kIndex)
                {
                    weights[kIndex] = std::exp(logits[kIndex] - maxLogit);
                    normalizer += weights[kIndex];
                }

                for (std::size_t valueIndex = 0; valueIndex < valueHidden; ++valueIndex)
                {
                    float acc = 0.0F;
                    for (std::size_t kIndex = 0; kIndex < visibleTokens; ++kIndex)
                    {
                        const float weight =
                            normalizer > 0.0F ? (weights[kIndex] / normalizer) : 0.0F;
                        const std::size_t vOffset =
                            RowMajorOffset(group, kIndex, valueIndex, keyTokens, valueHidden);
                        acc += weight * effectiveValue[vOffset];
                    }

                    const std::size_t outOffset =
                        RowMajorOffset(group, q, valueIndex, queryTokens, valueHidden);
                    output[outOffset] = acc;
                }
            }
        }

        return output;
    }

} // namespace us4::runtime::backends::cpu_avx
