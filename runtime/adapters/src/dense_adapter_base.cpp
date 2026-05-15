#include "us4/runtime/adapters/dense_adapter_base.h"

#include <algorithm>
#include <cctype>
#include <utility>

namespace us4::runtime::adapters
{
    namespace
    {
        std::string Normalize(std::string_view value)
        {
            std::string lowered;
            lowered.reserve(value.size());
            for (const char character : value)
            {
                lowered.push_back(
                    static_cast<char>(std::tolower(static_cast<unsigned char>(character))));
            }
            return lowered;
        }

        bool ContainsNormalized(std::string_view haystack, std::string_view needle)
        {
            const std::string loweredHaystack = Normalize(haystack);
            const std::string loweredNeedle = Normalize(needle);
            return loweredHaystack.find(loweredNeedle) != std::string::npos;
        }

        std::size_t EffectiveTokenBudget(const backends::SessionRequest& request,
                                         const DenseAdapterConfig& config)
        {
            const std::size_t requested = static_cast<std::size_t>(request.maxContextTokens);
            if (config.maxContextTokens == 0)
            {
                return requested;
            }
            return std::min(requested, config.maxContextTokens);
        }
    } // namespace

    DenseAdapterBase::DenseAdapterBase(DenseAdapterConfig config) : config_(std::move(config)) {}

    AdapterDescriptor DenseAdapterBase::Describe() const
    {
        return AdapterDescriptor{
            .adapterId = config_.adapterId,
            .family = config_.family,
            .revision = config_.revision,
            .features = config_.features,
            .preferredPrecision = config_.preferredPrecision,
            .maxContextTokens = config_.maxContextTokens,
            .maxBatchSize = config_.maxBatchSize,
            .supportsGqa = config_.supportsGqa,
            .supportsSlidingWindowAttention = config_.supportsSlidingWindowAttention,
            .supportsRoPE = config_.supportsRoPE,
        };
    }

    backends::RuntimeStatus DenseAdapterBase::Status() const
    {
        return status_;
    }

    AdapterHealth DenseAdapterBase::Health() const
    {
        AdapterHealth health{
            .lifecycle = lifecycle_,
        };
        if (!binding_.has_value())
        {
            health.issues.push_back(backends::RuntimeIssue{
                .code = "adapter-detached",
                .message = "Adapter has not been attached to a runtime binding yet.",
                .recoverable = true,
            });
        }
        if (loadedModelPath_.empty())
        {
            health.issues.push_back(backends::RuntimeIssue{
                .code = "model-unloaded",
                .message = "Adapter model weights have not been loaded yet.",
                .recoverable = true,
            });
        }
        if (binding_.has_value() && !AcceptsModelId(binding_->modelId))
        {
            health.issues.push_back(backends::RuntimeIssue{
                .code = "model-family-mismatch",
                .message = "Binding model id does not match this dense adapter family scaffold.",
                .recoverable = true,
            });
        }
        if (status_ == backends::RuntimeStatus::kError)
        {
            health.issues.push_back(backends::RuntimeIssue{
                .code = "adapter-faulted",
                .message = "Adapter entered an error state and requires Reset().",
                .recoverable = true,
            });
        }
        return health;
    }

    std::optional<RuntimeBinding> DenseAdapterBase::Binding() const
    {
        return binding_;
    }

    bool DenseAdapterBase::CanServe(const backends::SessionRequest& request) const
    {
        return binding_.has_value() && !loadedModelPath_.empty() &&
               AcceptsModelId(request.modelId) && request.maxGenerationTokens > 0 &&
               request.maxContextTokens <= Describe().maxContextTokens;
    }

    ModelResidencyPlan
    DenseAdapterBase::BuildResidencyPlan(const backends::SessionRequest& request) const
    {
        const std::size_t tokenBudget = EffectiveTokenBudget(request, config_);
        const std::size_t safeTokenBudget = std::max<std::size_t>(tokenBudget, 1);

        return ModelResidencyPlan{
            .modelId = request.modelId,
            .backendName = binding_.has_value() ? binding_->backend.name : "cpu-avx2",
            .profileId = binding_.has_value() ? binding_->profileId : "balanced",
            .expectedHostBytes = EstimateHostBytes(request),
            .expectedDeviceBytes = EstimateDeviceBytes(request),
            .kvBytesPerToken = config_.kvBytesPerToken,
            .expectedPrefillScratchBytes = config_.prefillScratchBytesPerToken * safeTokenBudget,
            .expectedDecodeScratchBytes = config_.decodeScratchBytesPerToken *
                                          static_cast<std::size_t>(request.maxGenerationTokens),
            .enableKvTiering = config_.features.supportsKvTiering,
            .enableSpeculative =
                config_.features.supportsSpeculativeDecoding && request.enableSpeculative,
        };
    }

    bool DenseAdapterBase::Attach(RuntimeBinding binding)
    {
        if (!AcceptsModelId(binding.modelId))
        {
            status_ = backends::RuntimeStatus::kError;
            lifecycle_ = AdapterLifecycle::kFaulted;
            return false;
        }

        binding_ = std::move(binding);
        lifecycle_ =
            loadedModelPath_.empty() ? AdapterLifecycle::kBound : AdapterLifecycle::kModelLoaded;
        status_ = backends::RuntimeStatus::kReady;
        return true;
    }

    bool DenseAdapterBase::LoadModel(const std::string& modelPath)
    {
        if (modelPath.empty())
        {
            status_ = backends::RuntimeStatus::kError;
            lifecycle_ = AdapterLifecycle::kFaulted;
            loadedModelPath_.clear();
            return false;
        }

        loadedModelPath_ = modelPath;
        lifecycle_ = AdapterLifecycle::kModelLoaded;
        status_ = binding_.has_value() ? backends::RuntimeStatus::kReady
                                       : backends::RuntimeStatus::kLoading;
        if (!binding_.has_value())
        {
            status_ = backends::RuntimeStatus::kIdle;
        }
        return true;
    }

    backends::TokenChunk DenseAdapterBase::Prefill(const std::string& prompt)
    {
        backends::TokenChunk chunk;
        if (!prompt.empty())
        {
            chunk.tokens.push_back(EncodePromptTokenEstimate(prompt));
        }
        lastPrefillTokens_ = chunk.tokens.size();
        return chunk;
    }

    backends::TokenChunk DenseAdapterBase::Generate(const backends::SessionRequest& request)
    {
        backends::TokenChunk chunk;
        if (!CanServe(request))
        {
            status_ = backends::RuntimeStatus::kError;
            lifecycle_ = AdapterLifecycle::kFaulted;
            return chunk;
        }

        status_ = backends::RuntimeStatus::kGenerating;
        lifecycle_ = AdapterLifecycle::kServing;

        chunk.tokens.push_back(EmitTerminalToken(request));
        chunk.isTerminal = true;

        lastFrame_ = GenerationFrame{
            .prefillTokens = lastPrefillTokens_,
            .decodeTokens = chunk.tokens.size(),
            .reachedEos = chunk.isTerminal,
        };

        lifecycle_ = AdapterLifecycle::kModelLoaded;
        status_ = backends::RuntimeStatus::kReady;
        return chunk;
    }

    GenerationFrame DenseAdapterBase::LastGenerationFrame() const
    {
        return lastFrame_;
    }

    void DenseAdapterBase::Reset()
    {
        binding_.reset();
        loadedModelPath_.clear();
        lastPrefillTokens_ = 0;
        lastFrame_ = GenerationFrame{};
        lifecycle_ = AdapterLifecycle::kDetached;
        status_ = backends::RuntimeStatus::kIdle;
    }

    const DenseAdapterConfig& DenseAdapterBase::Config() const
    {
        return config_;
    }

    bool DenseAdapterBase::AcceptsModelId(std::string_view modelId) const
    {
        return std::any_of(config_.modelAliases.begin(), config_.modelAliases.end(),
                           [modelId](const std::string& alias)
                           { return ContainsNormalized(modelId, alias); });
    }

    std::int32_t DenseAdapterBase::EncodePromptTokenEstimate(std::string_view prompt) const
    {
        const std::size_t divisor = std::max<std::size_t>(config_.promptTokenDivisor, 1);
        return static_cast<std::int32_t>(std::max<std::size_t>(1, prompt.size() / divisor));
    }

    std::int32_t DenseAdapterBase::EmitTerminalToken(const backends::SessionRequest& request) const
    {
        return config_.terminalTokenBase +
               static_cast<std::int32_t>(std::min<std::uint32_t>(request.maxGenerationTokens, 999));
    }

    std::size_t DenseAdapterBase::EstimateHostBytes(const backends::SessionRequest& request) const
    {
        const std::size_t tokenBudget = EffectiveTokenBudget(request, config_);
        return config_.hostBytesPerToken * std::max<std::size_t>(tokenBudget, 1);
    }

    std::size_t DenseAdapterBase::EstimateDeviceBytes(const backends::SessionRequest& request) const
    {
        const std::size_t tokenBudget = EffectiveTokenBudget(request, config_);
        if (binding_.has_value() && binding_->backend.kind == backends::BackendKind::kCpu)
        {
            return config_.deviceBytesPerToken * std::max<std::size_t>(tokenBudget, 1) / 4U;
        }
        return config_.deviceBytesPerToken * std::max<std::size_t>(tokenBudget, 1);
    }

} // namespace us4::runtime::adapters
