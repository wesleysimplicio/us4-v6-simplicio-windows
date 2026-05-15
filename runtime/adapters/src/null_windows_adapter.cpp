#include "us4/runtime/adapters/i_us4_windows_adapter.h"

#include <cstdint>
#include <optional>
#include <utility>

namespace us4::runtime::adapters
{

    namespace
    {

        class NullWindowsAdapter final : public IUS4WindowsAdapter
        {
          public:
            [[nodiscard]] AdapterDescriptor Describe() const override
            {
                return AdapterDescriptor{
                    .adapterId = "null",
                    .family = "null",
                    .revision = "0",
                    .features =
                        AdapterFeatureFlags{
                            .supportsContinuousBatching = false,
                            .supportsToolCalling = false,
                            .supportsVision = false,
                            .supportsEmbeddings = false,
                            .supportsStructuredOutput = false,
                            .supportsKvTiering = true,
                            .supportsSpeculativeDecoding = true,
                            .supportsMoE = false,
                            .supportsDeterministicReplay = true,
                        },
                    .preferredPrecision = backends::PrecisionMode::kFp32,
                    .maxContextTokens = 4096,
                    .maxBatchSize = 1,
                    .supportsGqa = false,
                    .supportsSlidingWindowAttention = false,
                    .supportsRoPE = false,
                };
            }

            [[nodiscard]] backends::RuntimeStatus Status() const override
            {
                return status_;
            }

            [[nodiscard]] AdapterHealth Health() const override
            {
                AdapterHealth health{
                    .lifecycle = lifecycle_,
                };
                if (status_ == backends::RuntimeStatus::kError)
                {
                    health.issues.push_back(backends::RuntimeIssue{
                        .code = "null-adapter-model-path",
                        .message = "Model path was empty for the current binding.",
                        .recoverable = true,
                    });
                }
                if (!binding_.has_value())
                {
                    health.issues.push_back(backends::RuntimeIssue{
                        .code = "null-adapter-detached",
                        .message = "Adapter has not been attached to a backend binding yet.",
                        .recoverable = true,
                    });
                }
                return health;
            }

            [[nodiscard]] std::optional<RuntimeBinding> Binding() const override
            {
                return binding_;
            }

            [[nodiscard]] bool CanServe(const backends::SessionRequest& request) const override
            {
                return binding_.has_value() && !modelPath_.empty() &&
                       request.maxGenerationTokens > 0;
            }

            [[nodiscard]] ModelResidencyPlan
            BuildResidencyPlan(const backends::SessionRequest& request) const override
            {
                return ModelResidencyPlan{
                    .modelId = request.modelId,
                    .backendName = binding_.has_value() ? binding_->backend.name : "cpu-avx2",
                    .profileId = binding_.has_value() ? binding_->profileId : "balanced",
                    .expectedHostBytes = request.maxContextTokens * sizeof(std::int32_t) * 4U,
                    .expectedDeviceBytes = request.maxContextTokens * sizeof(std::int32_t) * 2U,
                    .kvBytesPerToken = sizeof(std::int32_t) * 2U,
                    .expectedPrefillScratchBytes = request.maxContextTokens * sizeof(std::int32_t),
                    .expectedDecodeScratchBytes =
                        request.maxGenerationTokens * sizeof(std::int32_t),
                    .enableKvTiering = true,
                    .enableSpeculative = request.enableSpeculative,
                };
            }

            bool Attach(RuntimeBinding binding) override
            {
                binding_ = std::move(binding);
                lifecycle_ = AdapterLifecycle::kBound;
                status_ = backends::RuntimeStatus::kReady;
                return true;
            }

            bool LoadModel(const std::string& modelPath) override
            {
                modelPath_ = modelPath;
                status_ = modelPath_.empty() ? backends::RuntimeStatus::kError
                                             : backends::RuntimeStatus::kReady;
                lifecycle_ = modelPath_.empty() ? AdapterLifecycle::kFaulted
                                                : AdapterLifecycle::kModelLoaded;
                return !modelPath_.empty();
            }

            [[nodiscard]] backends::TokenChunk Prefill(const std::string& prompt) override
            {
                backends::TokenChunk chunk;
                if (!prompt.empty())
                {
                    chunk.tokens.push_back(static_cast<std::int32_t>(prompt.size()));
                }
                lastPrefillTokens_ = chunk.tokens.size();
                return chunk;
            }

            [[nodiscard]] backends::TokenChunk
            Generate(const backends::SessionRequest& request) override
            {
                status_ = backends::RuntimeStatus::kGenerating;
                lifecycle_ = AdapterLifecycle::kServing;
                backends::TokenChunk chunk;
                chunk.tokens.push_back(static_cast<std::int32_t>(request.maxGenerationTokens));
                chunk.isTerminal = true;
                lastFrame_ = GenerationFrame{
                    .prefillTokens = lastPrefillTokens_,
                    .decodeTokens = chunk.tokens.size(),
                    .reachedEos = chunk.isTerminal,
                };
                status_ = backends::RuntimeStatus::kReady;
                lifecycle_ = AdapterLifecycle::kModelLoaded;
                return chunk;
            }

            [[nodiscard]] GenerationFrame LastGenerationFrame() const override
            {
                return lastFrame_;
            }

            void Reset() override
            {
                binding_.reset();
                modelPath_.clear();
                lastPrefillTokens_ = 0;
                lastFrame_ = GenerationFrame{};
                lifecycle_ = AdapterLifecycle::kDetached;
                status_ = backends::RuntimeStatus::kIdle;
            }

          private:
            std::optional<RuntimeBinding> binding_;
            std::string modelPath_;
            std::size_t lastPrefillTokens_ = 0;
            GenerationFrame lastFrame_;
            AdapterLifecycle lifecycle_ = AdapterLifecycle::kDetached;
            backends::RuntimeStatus status_ = backends::RuntimeStatus::kIdle;
        };

    } // namespace

    std::unique_ptr<IUS4WindowsAdapter> CreateNullAdapter()
    {
        return std::make_unique<NullWindowsAdapter>();
    }

} // namespace us4::runtime::adapters
