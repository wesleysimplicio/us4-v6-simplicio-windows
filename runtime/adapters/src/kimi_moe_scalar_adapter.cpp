#include "us4/runtime/adapters/dense_adapter_base.h"
#include "us4/runtime/adapters/scalar_family_adapters.h"

#include <memory>

namespace us4::runtime::adapters
{
    namespace
    {
        DenseAdapterConfig MakeKimiConfig()
        {
            return DenseAdapterConfig{
                .adapterId = "moe-kimi",
                .family = "kimi",
                .revision = "scalar-s08",
                .modelAliases = {"kimi", "kimi-k2", "moonshot"},
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
                .preferredPrecision = backends::PrecisionMode::kBf16,
                .maxContextTokens = 65536,
                .maxBatchSize = 16,
                .hostBytesPerToken = 8704,
                .deviceBytesPerToken = 4608,
                .kvBytesPerToken = 960,
                .prefillScratchBytesPerToken = 704,
                .decodeScratchBytesPerToken = 352,
                .terminalTokenBase = 340000,
                .promptTokenBase = 339000,
                .promptTokenDivisor = 3,
                .supportsGqa = true,
                .supportsSlidingWindowAttention = true,
                .supportsRoPE = true,
            };
        }

        class KimiMoEScalarAdapter final : public DenseAdapterBase
        {
          public:
            KimiMoEScalarAdapter() : DenseAdapterBase(MakeKimiConfig()) {}
        };
    } // namespace

    std::unique_ptr<IUS4WindowsAdapter> CreateKimiMoEScalarAdapter()
    {
        return std::make_unique<KimiMoEScalarAdapter>();
    }

} // namespace us4::runtime::adapters
