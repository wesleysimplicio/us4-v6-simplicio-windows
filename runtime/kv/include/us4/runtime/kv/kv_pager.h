#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace us4::runtime::kv
{

    enum class KvTier
    {
        kDevice,
        kHost,
        kStorage,
        kSummary,
    };

    [[nodiscard]] std::string ToString(KvTier tier);

    struct KvPagerBudget
    {
        std::size_t deviceBytes = 0;
        std::size_t hostBytes = 0;
        std::size_t storageBytes = 0;
        std::size_t summaryBytes = 0;
    };

    struct KvSegment
    {
        std::string sequenceId;
        std::size_t tokenCount = 0;
        std::size_t residentBytes = 0;
        KvTier tier = KvTier::kHost;
        bool pinned = false;
        std::size_t accessCount = 0;
        std::size_t lastAccessTick = 0;
        std::size_t summaryTokenCount = 0;
    };

    struct KvPagerStats
    {
        std::size_t segmentCount = 0;
        std::size_t pinnedSegmentCount = 0;
        std::size_t residentBytes = 0;
        std::size_t deviceBytes = 0;
        std::size_t hostBytes = 0;
        std::size_t storageBytes = 0;
        std::size_t summaryBytes = 0;
        std::size_t deviceHitCount = 0;
        std::size_t hostHitCount = 0;
        std::size_t storageHitCount = 0;
        std::size_t summaryHitCount = 0;
        std::size_t evictionCount = 0;
        std::size_t restoreCount = 0;
        std::size_t summarizeCount = 0;
    };

    class KvPager
    {
      public:
        void ConfigureBudget(KvPagerBudget budget);
        [[nodiscard]] bool Upsert(KvSegment segment);
        [[nodiscard]] bool Pin(const KvSegment& segment);
        [[nodiscard]] bool Append(const std::string& sequenceId, std::size_t appendedTokens,
                                  std::size_t appendedBytes, KvTier preferredTier,
                                  bool pinned = false);
        [[nodiscard]] bool Evict(const std::string& sequenceId, KvTier targetTier);
        [[nodiscard]] bool Summarize(const std::string& sequenceId, std::size_t retainedTokens,
                                     std::size_t retainedBytes);
        [[nodiscard]] std::optional<KvSegment> Touch(const std::string& sequenceId);
        [[nodiscard]] std::optional<KvSegment> Restore(const std::string& sequenceId,
                                                       KvTier targetTier);
        [[nodiscard]] std::size_t Rebalance();
        [[nodiscard]] std::optional<KvSegment> Find(const std::string& sequenceId) const;
        [[nodiscard]] KvPagerStats Stats() const;
        [[nodiscard]] std::vector<KvSegment> Snapshot() const;

      private:
        [[nodiscard]] std::size_t TierBytes(KvTier tier) const;
        [[nodiscard]] std::size_t TierBudget(KvTier tier) const;
        [[nodiscard]] std::optional<std::size_t> FindIndex(const std::string& sequenceId) const;
        [[nodiscard]] std::size_t EstimateSummaryBytes(const KvSegment& segment,
                                                       std::size_t retainedTokens) const;
        [[nodiscard]] bool MoveToTier(std::size_t index, KvTier targetTier);
        [[nodiscard]] std::optional<std::size_t> SelectVictim(KvTier tier) const;

        KvPagerBudget budget_;
        std::vector<KvSegment> segments_;
        std::size_t nextAccessTick_ = 1;
        std::size_t deviceHitCount_ = 0;
        std::size_t hostHitCount_ = 0;
        std::size_t storageHitCount_ = 0;
        std::size_t summaryHitCount_ = 0;
        std::size_t evictionCount_ = 0;
        std::size_t restoreCount_ = 0;
        std::size_t summarizeCount_ = 0;
    };

} // namespace us4::runtime::kv
