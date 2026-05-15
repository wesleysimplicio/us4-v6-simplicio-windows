#include "us4/runtime/adapters/dense_adapter_base.h"
#include "us4/runtime/adapters/scalar_family_adapters.h"

#include <memory>
#include <string_view>

namespace us4::runtime::adapters
{
    namespace
    {
        DenseAdapterConfig MakeGemmaConfig()
        {
            return DenseAdapterConfig{
                .adapterId = "dense-gemma",
                .family = "gemma",
                .revision = "scalar-s01",
                .modelAliases = {"gemma", "gemma-2", "gemma-3", "gemma-4"},
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
                .preferredPrecision = backends::PrecisionMode::kBf16,
                .maxContextTokens = 8192,
                .maxBatchSize = 4,
                .hostBytesPerToken = 5120,
                .deviceBytesPerToken = 2304,
                .kvBytesPerToken = 640,
                .prefillScratchBytesPerToken = 320,
                .decodeScratchBytesPerToken = 160,
                .terminalTokenBase = 256000,
                .promptTokenDivisor = 5,
                .supportsGqa = true,
                .supportsSlidingWindowAttention = false,
                .supportsRoPE = true,
            };
        }

        class GemmaScalarAdapter final : public DenseAdapterBase
        {
          public:
            GemmaScalarAdapter() : DenseAdapterBase(MakeGemmaConfig()) {}

          protected:
            [[nodiscard]] std::int32_t
            EmitTerminalToken(const backends::SessionRequest& request) const override
            {
                return DenseAdapterBase::EmitTerminalToken(request) + 7;
            }
        };
    } // namespace

    std::unique_ptr<IUS4WindowsAdapter> CreateGemmaScalarAdapter()
    {
        return std::make_unique<GemmaScalarAdapter>();
    }

} // namespace us4::runtime::adapters
