#include "us4/runtime/adapters/bitnet_reference_hooks.h"

#include <algorithm>
#include <cmath>

namespace us4::runtime::adapters
{

    BitNetPackedWeights PackBitNetReferenceWeights(const std::vector<float>& weights,
                                                   const std::size_t rows,
                                                   const std::size_t columns,
                                                   BitNetExecutionHints hints)
    {
        if (rows == 0 || columns == 0 || weights.size() != rows * columns)
        {
            return BitNetPackedWeights{
                .hints = hints,
                .rows = rows,
                .columns = columns,
            };
        }

        BitNetPackedWeights packed{
            .hints = hints,
            .rows = rows,
            .columns = columns,
        };
        packed.values.reserve(weights.size());

        for (const float weight : weights)
        {
            if (hints.weightEncoding == BitNetWeightEncoding::kBinarySign)
            {
                packed.values.push_back(weight < 0.0F ? static_cast<std::int8_t>(-1)
                                                      : static_cast<std::int8_t>(1));
                continue;
            }

            if (std::fabs(weight) < 0.25F)
            {
                packed.values.push_back(0);
            }
            else
            {
                packed.values.push_back(weight < 0.0F ? static_cast<std::int8_t>(-1)
                                                      : static_cast<std::int8_t>(1));
            }
        }

        return packed;
    }

    std::vector<float> RunBitNetReferenceMatVec(const BitNetPackedWeights& packedWeights,
                                                const std::vector<float>& activations)
    {
        if (packedWeights.columns != activations.size())
        {
            return {};
        }

        std::vector<float> output(packedWeights.rows, 0.0F);
        for (std::size_t row = 0; row < packedWeights.rows; ++row)
        {
            float accumulator = 0.0F;
            const std::size_t offset = row * packedWeights.columns;
            for (std::size_t column = 0; column < packedWeights.columns; ++column)
            {
                const float clamped =
                    std::clamp(activations[column], -packedWeights.hints.activationClamp,
                               packedWeights.hints.activationClamp);
                accumulator += static_cast<float>(packedWeights.values[offset + column]) * clamped;
            }
            output[row] = accumulator;
        }
        return output;
    }

    std::size_t EstimateBitNetPackedBytes(const BitNetModelDescriptor& descriptor)
    {
        const std::size_t projectionBytes =
            descriptor.hiddenSize * descriptor.intermediateSize / 2U;
        const std::size_t attentionBytes = descriptor.hiddenSize * descriptor.hiddenSize / 4U;
        const std::size_t metadataBytes =
            descriptor.blockCount * descriptor.hints.packAlignmentBytes;
        return projectionBytes + attentionBytes + metadataBytes;
    }

    DenseAdapterConfig MakeBitNetDenseAdapterConfig()
    {
        return DenseAdapterConfig{
            .adapterId = "bitnet-reference",
            .family = "bitnet",
            .revision = "scalar-s05",
            .modelAliases = {"bitnet", "bitnet-b1.58", "b1.58", "tmac"},
            .features =
                AdapterFeatureFlags{
                    .supportsContinuousBatching = true,
                    .supportsToolCalling = false,
                    .supportsVision = false,
                    .supportsEmbeddings = true,
                    .supportsStructuredOutput = true,
                    .supportsKvTiering = true,
                    .supportsSpeculativeDecoding = false,
                    .supportsMoE = false,
                    .supportsDeterministicReplay = true,
                },
            .preferredPrecision = backends::PrecisionMode::kInt8,
            .maxContextTokens = 8192,
            .maxBatchSize = 4,
            .hostBytesPerToken = 2048,
            .deviceBytesPerToken = 768,
            .kvBytesPerToken = 512,
            .prefillScratchBytesPerToken = 160,
            .decodeScratchBytesPerToken = 96,
            .terminalTokenBase = 401000,
            .promptTokenBase = 400000,
            .promptTokenDivisor = 3,
            .supportsGqa = true,
            .supportsSlidingWindowAttention = false,
            .supportsRoPE = true,
        };
    }

} // namespace us4::runtime::adapters
