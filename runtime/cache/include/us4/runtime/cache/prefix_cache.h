#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace us4::runtime::cache
{

    struct PrefixCacheEntry
    {
        std::string key;
        std::string sequenceId;
        std::size_t promptTokens = 0;
        std::size_t kvBytes = 0;
        std::size_t hitCount = 0;
        bool warm = false;
    };

    class PrefixCache
    {
      public:
        [[nodiscard]] static std::string BuildKey(std::string_view prompt,
                                                  std::size_t promptTokens);
        void Upsert(PrefixCacheEntry entry);
        [[nodiscard]] bool Contains(const std::string& key) const;
        [[nodiscard]] std::optional<PrefixCacheEntry> TryGet(const std::string& key);
        [[nodiscard]] std::size_t TotalKvBytes() const;
        [[nodiscard]] std::size_t WarmEntryCount() const;
        [[nodiscard]] bool Erase(const std::string& key);
        [[nodiscard]] std::size_t Size() const;
        [[nodiscard]] std::vector<PrefixCacheEntry> Snapshot() const;

      private:
        std::unordered_map<std::string, PrefixCacheEntry> entries_;
    };

} // namespace us4::runtime::cache
