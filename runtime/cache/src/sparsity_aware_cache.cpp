#include "us4/runtime/cache/sparsity_aware_cache.h"

#include <utility>

namespace us4::runtime::cache
{

    void SparsityAwareCache::Configure(const std::size_t channelBudget, const float sparsityThreshold)
    {
        channelBudget_ = channelBudget;
        sparsityThreshold_ = sparsityThreshold;
    }

    void SparsityAwareCache::Upsert(SparsityCacheEntry entry)
    {
        entries_[entry.key] = std::move(entry);
    }

    std::optional<SparsityCacheEntry> SparsityAwareCache::TryGet(const std::string& key)
    {
        const auto it = entries_.find(key);
        if (it == entries_.end())
        {
            missCount_ += 1;
            return std::nullopt;
        }
        it->second.hitCount += 1;
        hitCount_ += 1;
        return it->second;
    }

    bool SparsityAwareCache::Contains(const std::string& key) const
    {
        return entries_.find(key) != entries_.end();
    }

    SparsityCacheStats SparsityAwareCache::Stats() const
    {
        SparsityCacheStats stats{};
        stats.entryCount = entries_.size();
        stats.hitCount = hitCount_;
        stats.missCount = missCount_;
        float ratioSum = 0.0F;
        for (const auto& [_, entry] : entries_)
        {
            stats.residentBytes += entry.residentBytes;
            ratioSum += SparsityRatio(entry);
        }
        if (!entries_.empty())
        {
            stats.averageSparsity = ratioSum / static_cast<float>(entries_.size());
        }
        return stats;
    }

    std::vector<SparsityCacheEntry> SparsityAwareCache::Snapshot() const
    {
        std::vector<SparsityCacheEntry> snapshot;
        snapshot.reserve(entries_.size());
        for (const auto& [_, entry] : entries_)
        {
            snapshot.push_back(entry);
        }
        return snapshot;
    }

    std::size_t SparsityAwareCache::EvictBelowThreshold()
    {
        std::size_t evicted = 0;
        for (auto it = entries_.begin(); it != entries_.end();)
        {
            if (SparsityRatio(it->second) < sparsityThreshold_)
            {
                it = entries_.erase(it);
                evicted += 1;
            }
            else
            {
                ++it;
            }
        }
        return evicted;
    }

    float SparsityAwareCache::SparsityRatio(const SparsityCacheEntry& entry) const
    {
        if (entry.totalChannels == 0)
        {
            return 0.0F;
        }
        const auto inactive = entry.totalChannels - entry.activeChannels;
        return static_cast<float>(inactive) / static_cast<float>(entry.totalChannels);
    }

} // namespace us4::runtime::cache
