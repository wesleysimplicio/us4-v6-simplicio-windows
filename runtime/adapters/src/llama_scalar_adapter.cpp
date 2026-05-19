#include "us4/runtime/adapters/dense_adapter_base.h"
#include "us4/runtime/adapters/llama_model_loader.h"
#include "us4/runtime/adapters/scalar_family_adapters.h"

#include <memory>
#include <optional>
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

            bool LoadModel(const std::string& modelPath) override
            {
                const auto binding = Binding();
                const auto modelId = binding.has_value() ? binding->modelId : "llama";
                const auto loadResult = LoadLlamaModelAsset(modelPath, modelId);
                if (!loadResult.ok)
                {
                    loadedDescriptor_.reset();
                    return DenseAdapterBase::LoadModel(modelPath);
                }

                loadedDescriptor_ = loadResult.descriptor;
                return CommitLoadedModel(loadResult.descriptor.asset);
            }

            void Reset() override
            {
                loadedDescriptor_.reset();
                DenseAdapterBase::Reset();
            }

          protected:
            [[nodiscard]] std::int32_t
            EncodePromptTokenEstimate(std::string_view prompt) const override
            {
                return DenseAdapterBase::EncodePromptTokenEstimate(prompt) + 42;
            }

            [[nodiscard]] std::optional<std::vector<std::int32_t>>
            TryTokenizePrompt(std::string_view prompt) const override
            {
                if (!loadedDescriptor_.has_value())
                {
                    return std::nullopt;
                }

                return TokenizeLlamaPrompt(loadedDescriptor_->tokenizer, prompt);
            }

            [[nodiscard]] std::optional<std::string>
            TryDetokenizePromptTokens(const std::vector<std::int32_t>& tokens) const override
            {
                if (!loadedDescriptor_.has_value())
                {
                    return std::nullopt;
                }

                return DetokenizeLlamaTokens(loadedDescriptor_->tokenizer, tokens);
            }

          private:
            std::optional<LlamaModelDescriptor> loadedDescriptor_;
        };
    } // namespace

    std::unique_ptr<IUS4WindowsAdapter> CreateLlamaScalarAdapter()
    {
        return std::make_unique<LlamaScalarAdapter>();
    }

} // namespace us4::runtime::adapters
