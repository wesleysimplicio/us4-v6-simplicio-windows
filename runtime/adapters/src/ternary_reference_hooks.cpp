#include "us4/runtime/adapters/ternary_reference_hooks.h"

#include <algorithm>
#include <cmath>

namespace us4::runtime::adapters
{

    TernaryPackedWeights PackTernaryReferenceWeights(const std::vector<float>& weights,
                                                     const std::size_t rows,
                                                     const std::size_t columns,
                                                     TernaryExecutionHints hints)
    {
        if (rows == 0 || columns == 0 || weights.size() != rows * columns)
        {
            return TernaryPackedWeights{
                .hints = hints,
                .rows = rows,
                .columns = columns,
            };
        }

        TernaryPackedWeights packed{
            .hints = hints,
            .rows = rows,
            .columns = columns,
        };
        packed.values.reserve(weights.size());

        for (const float weight : weights)
        {
            if (std::fabs(weight) < 0.2F)
            {
                packed.values.push_back(0);
            }
            else if (weight < 0.0F)
            {
                packed.values.push_back(-1);
            }
            else
            {
                packed.values.push_back(1);
            }
        }

        return packed;
    }

    std::vector<float> RunTernaryReferenceMatVec(const TernaryPackedWeights& packedWeights,
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

    std::size_t EstimateTernaryPackedBytes(const TernaryModelDescriptor& descriptor)
    {
        const std::size_t projectionBytes =
            descriptor.hiddenSize * descriptor.intermediateSize / 3U;
        const std::size_t attentionBytes = descriptor.hiddenSize * descriptor.hiddenSize / 3U;
        const std::size_t metadataBytes =
            descriptor.blockCount * descriptor.hints.packAlignmentBytes;
        return projectionBytes + attentionBytes + metadataBytes;
    }

    DenseAdapterConfig MakeTernaryDenseAdapterConfig()
    {
        return DenseAdapterConfig{
            .adapterId = "ternary-reference",
            .family = "ternary",
            .revision = "scalar-s05",
            .modelAliases = {"ternary", "t-mac", "tmac", "trit"},
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
            .maxContextTokens = 16384,
            .maxBatchSize = 8,
            .hostBytesPerToken = 2560,
            .deviceBytesPerToken = 1024,
            .kvBytesPerToken = 640,
            .prefillScratchBytesPerToken = 192,
            .decodeScratchBytesPerToken = 112,
            .terminalTokenBase = 451000,
            .promptTokenBase = 450000,
            .promptTokenDivisor = 3,
            .supportsGqa = true,
            .supportsSlidingWindowAttention = false,
            .supportsRoPE = true,
        };
    }

} // namespace us4::runtime::adapters
