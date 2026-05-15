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

    } // namespace

    AttentionValidation ValidateScalarAttention(const us4::core::Tensor& query,
                                                const us4::core::Tensor& key,
                                                const us4::core::Tensor& value)
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

        return {true, {}};
    }

    us4::core::Tensor ScalarAttention(const us4::core::Tensor& query, const us4::core::Tensor& key,
                                      const us4::core::Tensor& value, const float scale)
    {
        const AttentionValidation validation = ValidateScalarAttention(query, key, value);
        if (!validation.ok)
        {
            throw std::invalid_argument(validation.message);
        }

        const std::size_t rank = query.Rank();
        const std::size_t queryTokens = query.Dim(rank - 2);
        const std::size_t keyTokens = key.Dim(rank - 2);
        const std::size_t hiddenSize = query.Dim(rank - 1);
        const std::size_t valueHidden = value.Dim(rank - 1);
        const std::size_t groups = ComputePrefixGroupCount(query);

        const float effectiveScale =
            scale > 0.0F ? scale : (1.0F / std::sqrt(static_cast<float>(hiddenSize)));

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
                float maxLogit = -std::numeric_limits<float>::infinity();
                for (std::size_t kIndex = 0; kIndex < keyTokens; ++kIndex)
                {
                    float dot = 0.0F;
                    for (std::size_t hidden = 0; hidden < hiddenSize; ++hidden)
                    {
                        const std::size_t qOffset =
                            RowMajorOffset(group, q, hidden, queryTokens, hiddenSize);
                        const std::size_t kOffset =
                            RowMajorOffset(group, kIndex, hidden, keyTokens, hiddenSize);
                        dot += query[qOffset] * key[kOffset];
                    }

                    logits[kIndex] = dot * effectiveScale;
                    if (logits[kIndex] > maxLogit)
                    {
                        maxLogit = logits[kIndex];
                    }
                }

                float normalizer = 0.0F;
                for (std::size_t kIndex = 0; kIndex < keyTokens; ++kIndex)
                {
                    weights[kIndex] = std::exp(logits[kIndex] - maxLogit);
                    normalizer += weights[kIndex];
                }

                for (std::size_t valueIndex = 0; valueIndex < valueHidden; ++valueIndex)
                {
                    float acc = 0.0F;
                    for (std::size_t kIndex = 0; kIndex < keyTokens; ++kIndex)
                    {
                        const float weight =
                            normalizer > 0.0F ? (weights[kIndex] / normalizer) : 0.0F;
                        const std::size_t vOffset =
                            RowMajorOffset(group, kIndex, valueIndex, keyTokens, valueHidden);
                        acc += weight * value[vOffset];
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
