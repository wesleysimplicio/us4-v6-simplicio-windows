#include "us4/runtime/adapters/dense_adapter_base.h"
#include "us4/runtime/adapters/scalar_family_adapters.h"

#include <memory>

namespace us4::runtime::adapters
{
    namespace
    {
        DenseAdapterConfig MakeMiniMaxConfig()
        {
            return DenseAdapterConfig{
                .adapterId = "moe-minimax",
                .family = "minimax",
                .revision = "scalar-s09",
                .modelAliases = {"minimax", "minimax-m2", "minimax-text"},
                .features =
                    AdapterFeatureFlags{
                        .supportsContinuousBatching = true,
                        .supportsToolCalling = true,
                        .supportsVision = true,
                        .supportsEmbeddings = true,
                        .supportsStructuredOutput = true,
                        .supportsKvTiering = true,
                        .supportsSpeculativeDecoding = true,
                        .supportsMoE = true,
                        .supportsDeterministicReplay = true,
                    },
                .preferredPrecision = backends::PrecisionMode::kFp16,
                .maxContextTokens = 131072,
                .maxBatchSize = 8,
                .hostBytesPerToken = 10240,
                .deviceBytesPerToken = 5632,
                .kvBytesPerToken = 1152,
                .prefillScratchBytesPerToken = 896,
                .decodeScratchBytesPerToken = 448,
                .terminalTokenBase = 330200,
                .promptTokenBase = 329200,
                .promptTokenDivisor = 3,
                .supportsGqa = true,
                .supportsSlidingWindowAttention = true,
                .supportsRoPE = true,
            };
        }

        class MiniMaxScalarAdapter final : public DenseAdapterBase
        {
          public:
            MiniMaxScalarAdapter() : DenseAdapterBase(MakeMiniMaxConfig()) {}
        };
    } // namespace

    std::unique_ptr<IUS4WindowsAdapter> CreateMiniMaxScalarAdapter()
    {
        return std::make_unique<MiniMaxScalarAdapter>();
    }

} // namespace us4::runtime::adapters
