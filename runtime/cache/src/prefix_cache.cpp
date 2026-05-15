#include "us4/runtime/cache/prefix_cache.h"

#include <utility>

namespace us4::runtime::cache
{

    void PrefixCache::Upsert(PrefixCacheEntry entry)
    {
        entries_[entry.key] = std::move(entry);
    }

    bool PrefixCache::Contains(const std::string& key) const
    {
        return entries_.find(key) != entries_.end();
    }

    std::optional<PrefixCacheEntry> PrefixCache::TryGet(const std::string& key)
    {
        const auto it = entries_.find(key);
        if (it == entries_.end())
        {
            return std::nullopt;
        }
        it->second.hitCount += 1;
        it->second.warm = true;
        return it->second;
    }

    std::size_t PrefixCache::TotalKvBytes() const
    {
        std::size_t totalBytes = 0;
        for (const auto& [key, entry] : entries_)
        {
            static_cast<void>(key);
            totalBytes += entry.kvBytes;
        }
        return totalBytes;
    }

    bool PrefixCache::Erase(const std::string& key)
    {
        return entries_.erase(key) > 0;
    }

    std::size_t PrefixCache::Size() const
    {
        return entries_.size();
    }

} // namespace us4::runtime::cache
