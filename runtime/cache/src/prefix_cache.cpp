#include "us4/runtime/cache/prefix_cache.h"

#include <functional>
#include <sstream>
#include <utility>

namespace us4::runtime::cache
{

    std::string PrefixCache::BuildKey(std::string_view prompt, const std::size_t promptTokens)
    {
        const std::size_t digest = std::hash<std::string_view>{}(prompt);
        std::ostringstream key;
        key << "prefix:" << digest << ':' << promptTokens;
        return key.str();
    }

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

    std::size_t PrefixCache::WarmEntryCount() const
    {
        std::size_t warmEntries = 0;
        for (const auto& [key, entry] : entries_)
        {
            static_cast<void>(key);
            if (entry.warm)
            {
                ++warmEntries;
            }
        }
        return warmEntries;
    }

    bool PrefixCache::Erase(const std::string& key)
    {
        return entries_.erase(key) > 0;
    }

    std::size_t PrefixCache::Size() const
    {
        return entries_.size();
    }

    std::vector<PrefixCacheEntry> PrefixCache::Snapshot() const
    {
        std::vector<PrefixCacheEntry> entries;
        entries.reserve(entries_.size());
        for (const auto& [key, entry] : entries_)
        {
            static_cast<void>(key);
            entries.push_back(entry);
        }
        return entries;
    }

} // namespace us4::runtime::cache
