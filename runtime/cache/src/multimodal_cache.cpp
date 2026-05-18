#include "us4/runtime/cache/multimodal_cache.h"

#include <utility>

namespace us4::runtime::cache
{

    std::string ToString(const MultimodalChannel channel)
    {
        switch (channel)
        {
            case MultimodalChannel::kImage:
                return "image";
            case MultimodalChannel::kAudio:
                return "audio";
            case MultimodalChannel::kVideo:
                return "video";
        }
        return "unknown";
    }

    void MultimodalCache::Configure(const std::size_t byteBudget)
    {
        byteBudget_ = byteBudget;
    }

    void MultimodalCache::Upsert(MultimodalCacheEntry entry)
    {
        entries_[entry.key] = std::move(entry);
    }

    std::optional<MultimodalCacheEntry> MultimodalCache::TryGet(const std::string& key)
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

    bool MultimodalCache::Contains(const std::string& key) const
    {
        return entries_.find(key) != entries_.end();
    }

    bool MultimodalCache::Erase(const std::string& key)
    {
        return entries_.erase(key) > 0;
    }

    MultimodalCacheStats MultimodalCache::Stats() const
    {
        MultimodalCacheStats stats{};
        stats.entryCount = entries_.size();
        stats.hitCount = hitCount_;
        stats.missCount = missCount_;
        for (const auto& [_, entry] : entries_)
        {
            stats.residentBytes += entry.residentBytes;
            switch (entry.channel)
            {
                case MultimodalChannel::kImage:
                    stats.imageEntries += 1;
                    break;
                case MultimodalChannel::kAudio:
                    stats.audioEntries += 1;
                    break;
                case MultimodalChannel::kVideo:
                    stats.videoEntries += 1;
                    break;
            }
        }
        return stats;
    }

    std::vector<MultimodalCacheEntry> MultimodalCache::Snapshot() const
    {
        std::vector<MultimodalCacheEntry> snapshot;
        snapshot.reserve(entries_.size());
        for (const auto& [_, entry] : entries_)
        {
            snapshot.push_back(entry);
        }
        return snapshot;
    }

} // namespace us4::runtime::cache
