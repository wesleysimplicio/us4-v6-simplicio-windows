#include "us4/runtime/adapters/dense_adapter_base.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
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

        kv::KvTier ToKvTier(const KvCacheTier tier)
        {
            switch (tier)
            {
            case KvCacheTier::kDevice:
                return kv::KvTier::kDevice;
            case KvCacheTier::kHost:
                return kv::KvTier::kHost;
            case KvCacheTier::kStorage:
                return kv::KvTier::kStorage;
            case KvCacheTier::kSummary:
                return kv::KvTier::kSummary;
            }

            return kv::KvTier::kHost;
        }

        KvCacheTier ToKvCacheTier(const kv::KvTier tier)
        {
            switch (tier)
            {
            case kv::KvTier::kDevice:
                return KvCacheTier::kDevice;
            case kv::KvTier::kHost:
                return KvCacheTier::kHost;
            case kv::KvTier::kStorage:
                return KvCacheTier::kStorage;
            case kv::KvTier::kSummary:
                return KvCacheTier::kSummary;
            }

            return KvCacheTier::kHost;
        }

        KvCacheEntry ToKvCacheEntry(const kv::KvSegment& segment)
        {
            return KvCacheEntry{
                .sequenceId = segment.sequenceId,
                .tokenCount = segment.tokenCount,
                .residentBytes = segment.residentBytes,
                .tier = ToKvCacheTier(segment.tier),
                .pinned = segment.pinned,
                .accessCount = segment.accessCount,
                .summaryTokenCount = segment.summaryTokenCount,
            };
        }

        void CopyPagerStats(const kv::KvPagerStats& stats, KvHookSnapshot& snapshot)
        {
            snapshot.segmentCount = stats.segmentCount;
            snapshot.pinnedSegmentCount = stats.pinnedSegmentCount;
            snapshot.residentBytes = stats.residentBytes;
            snapshot.deviceBytes = stats.deviceBytes;
            snapshot.hostBytes = stats.hostBytes;
            snapshot.storageBytes = stats.storageBytes;
            snapshot.summaryBytes = stats.summaryBytes;
            snapshot.deviceHitCount = stats.deviceHitCount;
            snapshot.hostHitCount = stats.hostHitCount;
            snapshot.storageHitCount = stats.storageHitCount;
            snapshot.summaryHitCount = stats.summaryHitCount;
            snapshot.evictionPagerCount = stats.evictionCount;
            snapshot.restoreCount = stats.restoreCount;
            snapshot.pagerSummarizeCount = stats.summarizeCount;
        }
    } // namespace

    DenseAdapterBase::DenseAdapterBase(DenseAdapterConfig config) : config_(std::move(config))
    {
        ConfigureKvPagerBudget();
    }

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
        if (!loadedModel_.has_value())
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
        return binding_.has_value() && loadedModel_.has_value() &&
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
            .expectedHostBytes =
                std::max<std::size_t>(EstimateHostBytes(request),
                                      loadedModel_.has_value() ? loadedModel_->fileBytes : 0U),
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
        ConfigureKvPagerBudget();
        lifecycle_ =
            loadedModel_.has_value() ? AdapterLifecycle::kModelLoaded : AdapterLifecycle::kBound;
        status_ = backends::RuntimeStatus::kReady;
        return true;
    }

    bool DenseAdapterBase::LoadModel(const std::string& modelPath)
    {
        const ModelLoadResult loadResult =
            LoadModelAsset(modelPath, binding_.has_value() ? binding_->modelId : config_.adapterId);
        if (!loadResult.ok)
        {
            MarkModelLoadFailed();
            return false;
        }

        return CommitLoadedModel(std::move(loadResult.descriptor));
    }

    std::vector<std::int32_t> DenseAdapterBase::TokenizePrompt(std::string_view prompt) const
    {
        if (const auto specialized = TryTokenizePrompt(prompt); specialized.has_value())
        {
            return *specialized;
        }

        std::vector<std::int32_t> tokens;
        tokens.reserve(prompt.size());
        for (const unsigned char byte : std::string(prompt))
        {
            tokens.push_back(EncodePromptByte(byte));
        }
        if (tokens.empty())
        {
            tokens.push_back(EncodePromptTokenEstimate(prompt));
        }
        return tokens;
    }

    std::string
    DenseAdapterBase::DetokenizePromptTokens(const std::vector<std::int32_t>& tokens) const
    {
        if (const auto specialized = TryDetokenizePromptTokens(tokens); specialized.has_value())
        {
            return *specialized;
        }

        std::string prompt;
        prompt.reserve(tokens.size());
        for (const std::int32_t token : tokens)
        {
            char decoded = '\0';
            if (TryDecodePromptToken(token, decoded))
            {
                prompt.push_back(decoded);
            }
        }
        return prompt;
    }

    backends::TokenChunk DenseAdapterBase::Prefill(const std::string& prompt)
    {
        backends::TokenChunk chunk;
        if (!prompt.empty())
        {
            chunk.tokens = TokenizePrompt(prompt);
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
        const std::size_t tokenCount =
            std::max<std::size_t>(1U, std::min<std::uint32_t>(request.maxGenerationTokens, 8U));
        const std::int32_t baseToken = EmitTerminalToken(request);
        for (std::size_t index = 0; index < tokenCount; ++index)
        {
            chunk.tokens.push_back(baseToken + static_cast<std::int32_t>(index));
        }
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

    bool DenseAdapterBase::AppendKvCache(const std::string& sequenceId,
                                         const std::size_t appendedTokens,
                                         const std::size_t appendedBytes,
                                         const KvCacheTier preferredTier, const bool pinned)
    {
        const bool appended =
            kvPager_.Append(sequenceId, appendedTokens, appendedBytes, ToKvTier(preferredTier),
                            pinned);
        if (appended)
        {
            kvHooks_.appendCount += 1U;
            CopyPagerStats(kvPager_.Stats(), kvHooks_);
        }
        return appended;
    }

    std::optional<KvCacheEntry> DenseAdapterBase::LookupKvCache(const std::string& sequenceId)
    {
        const auto segment = kvPager_.Touch(sequenceId);
        if (segment.has_value())
        {
            kvHooks_.lookupCount += 1U;
            CopyPagerStats(kvPager_.Stats(), kvHooks_);
            return ToKvCacheEntry(*segment);
        }
        return std::nullopt;
    }

    bool DenseAdapterBase::EvictKvCache(const std::string& sequenceId,
                                        const KvCacheTier targetTier)
    {
        const bool evicted = kvPager_.Evict(sequenceId, ToKvTier(targetTier));
        if (evicted)
        {
            kvHooks_.evictCount += 1U;
            CopyPagerStats(kvPager_.Stats(), kvHooks_);
        }
        return evicted;
    }

    bool DenseAdapterBase::SummarizeKvCache(const std::string& sequenceId,
                                            const std::size_t retainedTokens,
                                            const std::size_t retainedBytes)
    {
        const bool summarized = kvPager_.Summarize(sequenceId, retainedTokens, retainedBytes);
        if (summarized)
        {
            kvHooks_.summarizeCount += 1U;
            CopyPagerStats(kvPager_.Stats(), kvHooks_);
        }
        return summarized;
    }

    KvHookSnapshot DenseAdapterBase::KvHooks() const
    {
        KvHookSnapshot snapshot = kvHooks_;
        CopyPagerStats(kvPager_.Stats(), snapshot);
        return snapshot;
    }

    void DenseAdapterBase::ResetKvCache()
    {
        kvPager_ = kv::KvPager{};
        kvHooks_ = KvHookSnapshot{};
        ConfigureKvPagerBudget();
    }

    GenerationFrame DenseAdapterBase::LastGenerationFrame() const
    {
        return lastFrame_;
    }

    void DenseAdapterBase::Reset()
    {
        binding_.reset();
        loadedModel_.reset();
        ResetKvCache();
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

    std::int32_t DenseAdapterBase::EncodePromptByte(unsigned char byte) const
    {
        return config_.promptTokenBase + static_cast<std::int32_t>(byte);
    }

    bool DenseAdapterBase::TryDecodePromptToken(std::int32_t token, char& decoded) const
    {
        const std::int32_t normalized = token - config_.promptTokenBase;
        if (normalized < 0 || normalized > 255)
        {
            return false;
        }

        decoded = static_cast<char>(normalized);
        return true;
    }

    std::optional<std::vector<std::int32_t>>
    DenseAdapterBase::TryTokenizePrompt(std::string_view /*prompt*/) const
    {
        return std::nullopt;
    }

    std::optional<std::string>
    DenseAdapterBase::TryDetokenizePromptTokens(const std::vector<std::int32_t>& /*tokens*/) const
    {
        return std::nullopt;
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
        const std::size_t baseEstimate =
            config_.deviceBytesPerToken * std::max<std::size_t>(tokenBudget, 1);
        if (binding_.has_value() && binding_->backend.kind == backends::BackendKind::kCpu)
        {
            return std::max<std::size_t>(
                baseEstimate / 4U, loadedModel_.has_value() ? loadedModel_->fileBytes / 16U : 0U);
        }
        return std::max<std::size_t>(baseEstimate,
                                     loadedModel_.has_value() ? loadedModel_->fileBytes / 2U : 0U);
    }

    void DenseAdapterBase::ConfigureKvPagerBudget()
    {
        const std::size_t hostBytes =
            std::max<std::size_t>(config_.hostBytesPerToken * 16U, config_.kvBytesPerToken * 16U);
        const bool prefersDevice =
            binding_.has_value() && binding_->backend.kind != backends::BackendKind::kCpu;
        const std::size_t deviceBytes =
            prefersDevice ? std::max<std::size_t>(config_.deviceBytesPerToken * 16U,
                                                  config_.kvBytesPerToken * 8U)
                          : 0U;
        const std::size_t storageBytes =
            std::max<std::size_t>(hostBytes * 2U, config_.kvBytesPerToken * 32U);
        const std::size_t summaryBytes =
            std::max<std::size_t>(config_.kvBytesPerToken * 8U, sizeof(std::int32_t) * 16U);
        kvPager_.ConfigureBudget(kv::KvPagerBudget{
            .deviceBytes = deviceBytes,
            .hostBytes = hostBytes,
            .storageBytes = storageBytes,
            .summaryBytes = summaryBytes,
        });
    }

    bool DenseAdapterBase::CommitLoadedModel(ModelAssetDescriptor descriptor)
    {
        loadedModel_ = std::move(descriptor);
        lifecycle_ = AdapterLifecycle::kModelLoaded;
        status_ = binding_.has_value() ? backends::RuntimeStatus::kReady
                                       : backends::RuntimeStatus::kLoading;
        if (!binding_.has_value())
        {
            status_ = backends::RuntimeStatus::kIdle;
        }
        return true;
    }

    void DenseAdapterBase::MarkModelLoadFailed()
    {
        status_ = backends::RuntimeStatus::kError;
        lifecycle_ = AdapterLifecycle::kFaulted;
        loadedModel_.reset();
    }

    void DenseAdapterBase::ClearLoadedModelState()
    {
        loadedModel_.reset();
    }

} // namespace us4::runtime::adapters
