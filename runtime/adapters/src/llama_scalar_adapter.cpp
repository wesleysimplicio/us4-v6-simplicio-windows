#include "us4/runtime/adapters/dense_adapter_base.h"
#include "us4/runtime/adapters/scalar_family_adapters.h"

#include <memory>
#include <string_view>

namespace us4::runtime::adapters
{
    namespace
    {
        DenseAdapterConfig MakeLlamaConfig()
        {
            return DenseAdapterConfig{
                .adapterId = "dense-llama",
                .family = "llama",
                .revision = "scalar-s02",
                .modelAliases = {"llama", "llama-3", "llama-4", "meta-llama"},
                .features =
                    AdapterFeatureFlags{
                        .supportsContinuousBatching = true,
                        .supportsToolCalling = false,
                        .supportsVision = false,
                        .supportsEmbeddings = true,
                        .supportsStructuredOutput = true,
                        .supportsKvTiering = true,
                        .supportsSpeculativeDecoding = true,
                        .supportsMoE = false,
                        .supportsDeterministicReplay = true,
                    },
                .preferredPrecision = backends::PrecisionMode::kFp16,
                .maxContextTokens = 32768,
                .maxBatchSize = 8,
                .hostBytesPerToken = 6400,
                .deviceBytesPerToken = 3328,
                .kvBytesPerToken = 896,
                .prefillScratchBytesPerToken = 448,
                .decodeScratchBytesPerToken = 224,
                .terminalTokenBase = 128256,
                .promptTokenBase = 128000,
                .promptTokenDivisor = 4,
                .supportsGqa = true,
                .supportsSlidingWindowAttention = false,
                .supportsRoPE = true,
            };
        }

        class LlamaScalarAdapter final : public DenseAdapterBase
        {
          public:
            LlamaScalarAdapter() : DenseAdapterBase(MakeLlamaConfig()) {}

          protected:
            [[nodiscard]] std::int32_t
            EncodePromptTokenEstimate(std::string_view prompt) const override
            {
                return DenseAdapterBase::EncodePromptTokenEstimate(prompt) + 42;
            }
        };
    } // namespace

    std::unique_ptr<IUS4WindowsAdapter> CreateLlamaScalarAdapter()
    {
        return std::make_unique<LlamaScalarAdapter>();
    }

} // namespace us4::runtime::adapters
