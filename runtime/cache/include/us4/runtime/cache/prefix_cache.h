#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <unordered_map>

namespace us4::runtime::cache
{

    struct PrefixCacheEntry
    {
        std::string key;
        std::size_t promptTokens = 0;
        std::size_t kvBytes = 0;
        std::size_t hitCount = 0;
        bool warm = false;
    };

    class PrefixCache
    {
      public:
        void Upsert(PrefixCacheEntry entry);
        [[nodiscard]] bool Contains(const std::string& key) const;
        [[nodiscard]] std::optional<PrefixCacheEntry> TryGet(const std::string& key);
        [[nodiscard]] std::size_t TotalKvBytes() const;
        [[nodiscard]] bool Erase(const std::string& key);
        [[nodiscard]] std::size_t Size() const;

      private:
        std::unordered_map<std::string, PrefixCacheEntry> entries_;
    };

} // namespace us4::runtime::cache
