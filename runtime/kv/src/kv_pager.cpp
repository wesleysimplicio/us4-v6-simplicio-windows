#include "us4/runtime/kv/kv_pager.h"

#include <algorithm>

namespace us4::runtime::kv
{

    void KvPager::ConfigureBudget(KvPagerBudget budget)
    {
        budget_ = budget;
    }

    bool KvPager::Pin(const KvSegment& segment)
    {
        const auto it =
            std::find_if(segments_.begin(), segments_.end(), [&](const KvSegment& current)
                         { return current.sequenceId == segment.sequenceId; });
        KvSegment next = segment;
        next.pinned = true;
        if (it != segments_.end())
        {
            *it = next;
        }
        else
        {
            segments_.push_back(next);
        }
        return true;
    }

    bool KvPager::Evict(const std::string& sequenceId, const std::string& targetTier)
    {
        const auto it =
            std::find_if(segments_.begin(), segments_.end(), [&](const KvSegment& current)
                         { return current.sequenceId == sequenceId; });
        if (it == segments_.end())
        {
            return false;
        }
        it->tier = targetTier;
        it->pinned = false;
        return true;
    }

    std::optional<KvSegment> KvPager::Find(const std::string& sequenceId) const
    {
        const auto it =
            std::find_if(segments_.begin(), segments_.end(), [&](const KvSegment& current)
                         { return current.sequenceId == sequenceId; });
        if (it == segments_.end())
        {
            return std::nullopt;
        }
        return *it;
    }

    KvPagerStats KvPager::Stats() const
    {
        KvPagerStats stats{
            .segmentCount = segments_.size(),
        };
        for (const KvSegment& segment : segments_)
        {
            stats.residentBytes += segment.residentBytes;
            if (segment.pinned)
            {
                ++stats.pinnedSegmentCount;
            }
            if (segment.tier == "device")
            {
                stats.deviceBytes += segment.residentBytes;
            }
            else if (segment.tier == "storage")
            {
                stats.storageBytes += segment.residentBytes;
            }
            else
            {
                stats.hostBytes += segment.residentBytes;
            }
        }
        return stats;
    }

    std::vector<KvSegment> KvPager::Snapshot() const
    {
        return segments_;
    }

} // namespace us4::runtime::kv
