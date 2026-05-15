#include "us4/runtime/kv/kv_pager.h"

#include <algorithm>
#include <cstdint>

namespace us4::runtime::kv
{
    namespace
    {
        constexpr std::size_t kDefaultSummaryBytesPerToken = sizeof(std::int32_t);

        std::size_t TierOrdinal(KvTier tier)
        {
            switch (tier)
            {
            case KvTier::kDevice:
                return 0;
            case KvTier::kHost:
                return 1;
            case KvTier::kStorage:
                return 2;
            case KvTier::kSummary:
                return 3;
            }

            return 3;
        }

    } // namespace

    std::string ToString(KvTier tier)
    {
        switch (tier)
        {
        case KvTier::kDevice:
            return "device";
        case KvTier::kHost:
            return "host";
        case KvTier::kStorage:
            return "storage";
        case KvTier::kSummary:
            return "summary";
        }

        return "host";
    }

    void KvPager::ConfigureBudget(KvPagerBudget budget)
    {
        budget_ = budget;
    }

    bool KvPager::Upsert(KvSegment segment)
    {
        if (segment.sequenceId.empty())
        {
            return false;
        }

        segment.lastAccessTick = nextAccessTick_++;
        if (segment.summaryTokenCount == 0 && segment.tier == KvTier::kSummary)
        {
            segment.summaryTokenCount = segment.tokenCount;
        }

        const auto existingIndex = FindIndex(segment.sequenceId);
        if (existingIndex.has_value())
        {
            segments_[*existingIndex] = segment;
        }
        else
        {
            segments_.push_back(std::move(segment));
        }

        static_cast<void>(Rebalance());
        return true;
    }

    bool KvPager::Pin(const KvSegment& segment)
    {
        KvSegment next = segment;
        next.pinned = true;
        return Upsert(std::move(next));
    }

    bool KvPager::Append(const std::string& sequenceId, const std::size_t appendedTokens,
                         const std::size_t appendedBytes, const KvTier preferredTier,
                         const bool pinned)
    {
        if (sequenceId.empty())
        {
            return false;
        }

        const auto index = FindIndex(sequenceId);
        if (!index.has_value())
        {
            return Upsert(KvSegment{
                .sequenceId = sequenceId,
                .tokenCount = appendedTokens,
                .residentBytes = appendedBytes,
                .tier = preferredTier,
                .pinned = pinned,
                .accessCount = 1,
                .lastAccessTick = nextAccessTick_++,
                .summaryTokenCount = preferredTier == KvTier::kSummary ? appendedTokens : 0,
            });
        }

        KvSegment& segment = segments_[*index];
        segment.tokenCount += appendedTokens;
        segment.residentBytes += appendedBytes;
        segment.tier = preferredTier;
        segment.pinned = pinned;
        segment.accessCount += 1;
        segment.lastAccessTick = nextAccessTick_++;
        if (segment.tier != KvTier::kSummary)
        {
            segment.summaryTokenCount = 0;
        }

        static_cast<void>(Rebalance());
        return true;
    }

    bool KvPager::Evict(const std::string& sequenceId, const KvTier targetTier)
    {
        const auto index = FindIndex(sequenceId);
        if (!index.has_value())
        {
            return false;
        }

        return MoveToTier(*index, targetTier);
    }

    bool KvPager::Summarize(const std::string& sequenceId, const std::size_t retainedTokens,
                            const std::size_t retainedBytes)
    {
        const auto index = FindIndex(sequenceId);
        if (!index.has_value())
        {
            return false;
        }

        KvSegment& segment = segments_[*index];
        segment.tier = KvTier::kSummary;
        segment.summaryTokenCount = std::min(retainedTokens, segment.tokenCount);
        segment.residentBytes =
            retainedBytes > 0 ? retainedBytes : EstimateSummaryBytes(segment, retainedTokens);
        segment.pinned = false;
        segment.lastAccessTick = nextAccessTick_++;
        ++summarizeCount_;
        static_cast<void>(Rebalance());
        return true;
    }

    std::optional<KvSegment> KvPager::Touch(const std::string& sequenceId)
    {
        const auto index = FindIndex(sequenceId);
        if (!index.has_value())
        {
            return std::nullopt;
        }

        KvSegment& segment = segments_[*index];
        segment.accessCount += 1;
        segment.lastAccessTick = nextAccessTick_++;

        switch (segment.tier)
        {
        case KvTier::kDevice:
            ++deviceHitCount_;
            break;
        case KvTier::kHost:
            ++hostHitCount_;
            break;
        case KvTier::kStorage:
            ++storageHitCount_;
            break;
        case KvTier::kSummary:
            ++summaryHitCount_;
            break;
        }

        return segment;
    }

    std::optional<KvSegment> KvPager::Restore(const std::string& sequenceId,
                                              const KvTier targetTier)
    {
        const auto index = FindIndex(sequenceId);
        if (!index.has_value())
        {
            return std::nullopt;
        }

        if (!MoveToTier(*index, targetTier))
        {
            return std::nullopt;
        }

        ++restoreCount_;
        return segments_[*index];
    }

    std::size_t KvPager::Rebalance()
    {
        std::size_t movedSegments = 0;
        while (TierBytes(KvTier::kDevice) > TierBudget(KvTier::kDevice))
        {
            const auto victim = SelectVictim(KvTier::kDevice);
            if (!victim.has_value() || !MoveToTier(*victim, KvTier::kHost))
            {
                break;
            }
            ++movedSegments;
        }

        while (TierBytes(KvTier::kHost) > TierBudget(KvTier::kHost))
        {
            const auto victim = SelectVictim(KvTier::kHost);
            if (!victim.has_value() || !MoveToTier(*victim, KvTier::kStorage))
            {
                break;
            }
            ++movedSegments;
        }

        while (TierBytes(KvTier::kStorage) > TierBudget(KvTier::kStorage))
        {
            const auto victim = SelectVictim(KvTier::kStorage);
            if (!victim.has_value())
            {
                break;
            }

            KvSegment& segment = segments_[*victim];
            segment.tier = KvTier::kSummary;
            segment.summaryTokenCount = std::min<std::size_t>(segment.tokenCount, 32U);
            segment.residentBytes =
                EstimateSummaryBytes(segment, std::max<std::size_t>(segment.summaryTokenCount, 1U));
            segment.pinned = false;
            segment.lastAccessTick = nextAccessTick_++;
            ++summarizeCount_;
            ++evictionCount_;
            ++movedSegments;
        }

        return movedSegments;
    }

    std::optional<KvSegment> KvPager::Find(const std::string& sequenceId) const
    {
        const auto index = FindIndex(sequenceId);
        if (!index.has_value())
        {
            return std::nullopt;
        }
        return segments_[*index];
    }

    KvPagerStats KvPager::Stats() const
    {
        KvPagerStats stats{
            .segmentCount = segments_.size(),
            .deviceHitCount = deviceHitCount_,
            .hostHitCount = hostHitCount_,
            .storageHitCount = storageHitCount_,
            .summaryHitCount = summaryHitCount_,
            .evictionCount = evictionCount_,
            .restoreCount = restoreCount_,
            .summarizeCount = summarizeCount_,
        };

        for (const KvSegment& segment : segments_)
        {
            stats.residentBytes += segment.residentBytes;
            if (segment.pinned)
            {
                ++stats.pinnedSegmentCount;
            }

            switch (segment.tier)
            {
            case KvTier::kDevice:
                stats.deviceBytes += segment.residentBytes;
                break;
            case KvTier::kHost:
                stats.hostBytes += segment.residentBytes;
                break;
            case KvTier::kStorage:
                stats.storageBytes += segment.residentBytes;
                break;
            case KvTier::kSummary:
                stats.summaryBytes += segment.residentBytes;
                break;
            }
        }

        return stats;
    }

    std::vector<KvSegment> KvPager::Snapshot() const
    {
        return segments_;
    }

    std::size_t KvPager::TierBytes(const KvTier tier) const
    {
        std::size_t bytes = 0;
        for (const KvSegment& segment : segments_)
        {
            if (segment.tier == tier)
            {
                bytes += segment.residentBytes;
            }
        }
        return bytes;
    }

    std::size_t KvPager::TierBudget(const KvTier tier) const
    {
        switch (tier)
        {
        case KvTier::kDevice:
            return budget_.deviceBytes;
        case KvTier::kHost:
            return budget_.hostBytes;
        case KvTier::kStorage:
            return budget_.storageBytes;
        case KvTier::kSummary:
            return budget_.summaryBytes;
        }

        return 0;
    }

    std::optional<std::size_t> KvPager::FindIndex(const std::string& sequenceId) const
    {
        const auto it =
            std::find_if(segments_.begin(), segments_.end(), [&](const KvSegment& current)
                         { return current.sequenceId == sequenceId; });
        if (it == segments_.end())
        {
            return std::nullopt;
        }

        return static_cast<std::size_t>(std::distance(segments_.begin(), it));
    }

    std::size_t KvPager::EstimateSummaryBytes(const KvSegment& segment,
                                              const std::size_t retainedTokens) const
    {
        const std::size_t effectiveTokens = std::max<std::size_t>(retainedTokens, 1U);
        return std::min(segment.residentBytes, effectiveTokens * kDefaultSummaryBytesPerToken);
    }

    bool KvPager::MoveToTier(const std::size_t index, const KvTier targetTier)
    {
        if (index >= segments_.size())
        {
            return false;
        }

        KvSegment& segment = segments_[index];
        if (segment.tier == targetTier)
        {
            return true;
        }

        segment.tier = targetTier;
        segment.lastAccessTick = nextAccessTick_++;
        segment.pinned = false;
        if (targetTier == KvTier::kSummary)
        {
            segment.summaryTokenCount = std::min<std::size_t>(segment.tokenCount, 32U);
            segment.residentBytes =
                EstimateSummaryBytes(segment, std::max<std::size_t>(segment.summaryTokenCount, 1U));
            ++summarizeCount_;
        }
        else
        {
            segment.summaryTokenCount = 0;
        }

        ++evictionCount_;
        return true;
    }

    std::optional<std::size_t> KvPager::SelectVictim(const KvTier tier) const
    {
        std::optional<std::size_t> victimIndex;
        for (std::size_t index = 0; index < segments_.size(); ++index)
        {
            const KvSegment& segment = segments_[index];
            if (segment.tier != tier || segment.pinned)
            {
                continue;
            }

            if (!victimIndex.has_value())
            {
                victimIndex = index;
                continue;
            }

            const KvSegment& currentVictim = segments_[*victimIndex];
            if (segment.accessCount < currentVictim.accessCount)
            {
                victimIndex = index;
                continue;
            }
            if (segment.accessCount == currentVictim.accessCount &&
                segment.lastAccessTick < currentVictim.lastAccessTick)
            {
                victimIndex = index;
                continue;
            }
            if (segment.accessCount == currentVictim.accessCount &&
                segment.lastAccessTick == currentVictim.lastAccessTick &&
                TierOrdinal(segment.tier) >= TierOrdinal(currentVictim.tier))
            {
                victimIndex = index;
            }
        }

        return victimIndex;
    }

} // namespace us4::runtime::kv
