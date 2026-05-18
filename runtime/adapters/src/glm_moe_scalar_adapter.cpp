#include "us4/runtime/adapters/dense_adapter_base.h"
#include "us4/runtime/adapters/scalar_family_adapters.h"

#include <memory>

namespace us4::runtime::adapters
{
    namespace
    {
        DenseAdapterConfig MakeGlmConfig()
        {
            return DenseAdapterConfig{
                .adapterId = "moe-glm",
                .family = "glm",
                .revision = "scalar-s09",
                .modelAliases = {"glm", "glm-4", "chatglm"},
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
                .maxContextTokens = 32768,
                .maxBatchSize = 12,
                .hostBytesPerToken = 9728,
                .deviceBytesPerToken = 5376,
                .kvBytesPerToken = 1088,
                .prefillScratchBytesPerToken = 832,
                .decodeScratchBytesPerToken = 416,
                .terminalTokenBase = 330400,
                .promptTokenBase = 329400,
                .promptTokenDivisor = 3,
                .supportsGqa = true,
                .supportsSlidingWindowAttention = false,
                .supportsRoPE = true,
            };
        }

        class GlmScalarAdapter final : public DenseAdapterBase
        {
          public:
            GlmScalarAdapter() : DenseAdapterBase(MakeGlmConfig()) {}
        };
    } // namespace

    std::unique_ptr<IUS4WindowsAdapter> CreateGlmScalarAdapter()
    {
        return std::make_unique<GlmScalarAdapter>();
    }

} // namespace us4::runtime::adapters
