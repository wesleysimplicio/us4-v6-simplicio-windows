#include "us4/runtime/adapters/dense_adapter_base.h"
#include "us4/runtime/adapters/scalar_family_adapters.h"

#include <memory>
#include <string_view>

namespace us4::runtime::adapters
{
    namespace
    {
        DenseAdapterConfig MakeQwenConfig()
        {
            return DenseAdapterConfig{
                .adapterId = "dense-qwen",
                .family = "qwen",
                .revision = "scalar-s01",
                .modelAliases = {"qwen", "qwen2", "qwen2.5", "qwen3", "qwen-coder"},
                .features =
                    AdapterFeatureFlags{
                        .supportsContinuousBatching = true,
                        .supportsToolCalling = true,
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
                .hostBytesPerToken = 6144,
                .deviceBytesPerToken = 3072,
                .kvBytesPerToken = 768,
                .prefillScratchBytesPerToken = 384,
                .decodeScratchBytesPerToken = 192,
                .terminalTokenBase = 151600,
                .promptTokenBase = 151000,
                .promptTokenDivisor = 3,
                .supportsGqa = true,
                .supportsSlidingWindowAttention = true,
                .supportsRoPE = true,
            };
        }

        class QwenScalarAdapter final : public DenseAdapterBase
        {
          public:
            QwenScalarAdapter() : DenseAdapterBase(MakeQwenConfig()) {}

          protected:
            [[nodiscard]] std::int32_t
            EncodePromptTokenEstimate(std::string_view prompt) const override
            {
                const auto baseEstimate = DenseAdapterBase::EncodePromptTokenEstimate(prompt);
                return baseEstimate + 151;
            }
        };
    } // namespace

    std::unique_ptr<IUS4WindowsAdapter> CreateQwenScalarAdapter()
    {
        return std::make_unique<QwenScalarAdapter>();
    }

} // namespace us4::runtime::adapters
