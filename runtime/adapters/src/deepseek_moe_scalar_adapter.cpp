#include "us4/runtime/adapters/dense_adapter_base.h"
#include "us4/runtime/adapters/scalar_family_adapters.h"

#include <memory>

namespace us4::runtime::adapters
{
    namespace
    {
        DenseAdapterConfig MakeDeepSeekConfig()
        {
            return DenseAdapterConfig{
                .adapterId = "moe-deepseek",
                .family = "deepseek",
                .revision = "scalar-s08",
                .modelAliases = {"deepseek", "deepseek-r1", "deepseek-v2", "deepseek-v3"},
                .features =
                    AdapterFeatureFlags{
                        .supportsContinuousBatching = true,
                        .supportsToolCalling = true,
                        .supportsVision = false,
                        .supportsEmbeddings = true,
                        .supportsStructuredOutput = true,
                        .supportsKvTiering = true,
                        .supportsSpeculativeDecoding = true,
                        .supportsMoE = true,
                        .supportsDeterministicReplay = true,
                    },
                .preferredPrecision = backends::PrecisionMode::kFp16,
                .maxContextTokens = 65536,
                .maxBatchSize = 16,
                .hostBytesPerToken = 9216,
                .deviceBytesPerToken = 5120,
                .kvBytesPerToken = 1024,
                .prefillScratchBytesPerToken = 768,
                .decodeScratchBytesPerToken = 384,
                .terminalTokenBase = 330000,
                .promptTokenBase = 329000,
                .promptTokenDivisor = 3,
                .supportsGqa = true,
                .supportsSlidingWindowAttention = true,
                .supportsRoPE = true,
            };
        }

        class DeepSeekMoEScalarAdapter final : public DenseAdapterBase
        {
          public:
            DeepSeekMoEScalarAdapter() : DenseAdapterBase(MakeDeepSeekConfig()) {}
        };
    } // namespace

    std::unique_ptr<IUS4WindowsAdapter> CreateDeepSeekMoEScalarAdapter()
    {
        return std::make_unique<DeepSeekMoEScalarAdapter>();
    }

} // namespace us4::runtime::adapters
