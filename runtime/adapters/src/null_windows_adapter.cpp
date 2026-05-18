#include "us4/runtime/adapters/i_us4_windows_adapter.h"
#include "us4/runtime/kv/kv_pager.h"

#include <cstdint>
#include <optional>
#include <utility>

namespace us4::runtime::adapters
{

    namespace
    {
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

        class NullWindowsAdapter final : public IUS4WindowsAdapter
        {
          public:
            NullWindowsAdapter()
            {
                ResetKvCache();
            }

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

            [[nodiscard]] bool AppendKvCache(const std::string& sequenceId,
                                             const std::size_t appendedTokens,
                                             const std::size_t appendedBytes,
                                             const KvCacheTier preferredTier,
                                             const bool pinned) override
            {
                const bool appended =
                    kvPager_.Append(sequenceId, appendedTokens, appendedBytes,
                                    ToKvTier(preferredTier), pinned);
                if (appended)
                {
                    kvHooks_.appendCount += 1U;
                    CopyPagerStats(kvPager_.Stats(), kvHooks_);
                }
                return appended;
            }

            [[nodiscard]] std::optional<KvCacheEntry>
            LookupKvCache(const std::string& sequenceId) override
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

            [[nodiscard]] bool EvictKvCache(const std::string& sequenceId,
                                            const KvCacheTier targetTier) override
            {
                const bool evicted = kvPager_.Evict(sequenceId, ToKvTier(targetTier));
                if (evicted)
                {
                    kvHooks_.evictCount += 1U;
                    CopyPagerStats(kvPager_.Stats(), kvHooks_);
                }
                return evicted;
            }

            [[nodiscard]] bool SummarizeKvCache(const std::string& sequenceId,
                                                const std::size_t retainedTokens,
                                                const std::size_t retainedBytes) override
            {
                const bool summarized =
                    kvPager_.Summarize(sequenceId, retainedTokens, retainedBytes);
                if (summarized)
                {
                    kvHooks_.summarizeCount += 1U;
                    CopyPagerStats(kvPager_.Stats(), kvHooks_);
                }
                return summarized;
            }

            [[nodiscard]] KvHookSnapshot KvHooks() const override
            {
                KvHookSnapshot snapshot = kvHooks_;
                CopyPagerStats(kvPager_.Stats(), snapshot);
                return snapshot;
            }

            void ResetKvCache() override
            {
                kvPager_ = kv::KvPager{};
                kvPager_.ConfigureBudget(kv::KvPagerBudget{
                    .deviceBytes = 0U,
                    .hostBytes = 4096U,
                    .storageBytes = 8192U,
                    .summaryBytes = 1024U,
                });
                kvHooks_ = KvHookSnapshot{};
            }

            [[nodiscard]] GenerationFrame LastGenerationFrame() const override
            {
                return lastFrame_;
            }

            void Reset() override
            {
                binding_.reset();
                modelPath_.clear();
                ResetKvCache();
                lastPrefillTokens_ = 0;
                lastFrame_ = GenerationFrame{};
                lifecycle_ = AdapterLifecycle::kDetached;
                status_ = backends::RuntimeStatus::kIdle;
            }

          private:
            std::optional<RuntimeBinding> binding_;
            std::string modelPath_;
            kv::KvPager kvPager_;
            KvHookSnapshot kvHooks_;
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
