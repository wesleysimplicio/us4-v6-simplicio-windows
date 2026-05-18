#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace us4::runtime::cache
{

    enum class MultimodalChannel
    {
        kImage,
        kAudio,
        kVideo,
    };

    [[nodiscard]] std::string ToString(MultimodalChannel channel);

    struct MultimodalCacheEntry
    {
        std::string key;
        MultimodalChannel channel = MultimodalChannel::kImage;
        std::size_t tokenCount = 0;
        std::size_t residentBytes = 0;
        std::size_t hitCount = 0;
    };

    struct MultimodalCacheStats
    {
        std::size_t entryCount = 0;
        std::size_t hitCount = 0;
        std::size_t missCount = 0;
        std::size_t residentBytes = 0;
        std::size_t imageEntries = 0;
        std::size_t audioEntries = 0;
        std::size_t videoEntries = 0;
    };

    class MultimodalCache
    {
      public:
        void Configure(std::size_t byteBudget);
        void Upsert(MultimodalCacheEntry entry);
        [[nodiscard]] std::optional<MultimodalCacheEntry> TryGet(const std::string& key);
        [[nodiscard]] bool Contains(const std::string& key) const;
        [[nodiscard]] bool Erase(const std::string& key);
        [[nodiscard]] MultimodalCacheStats Stats() const;
        [[nodiscard]] std::vector<MultimodalCacheEntry> Snapshot() const;

      private:
        std::unordered_map<std::string, MultimodalCacheEntry> entries_;
        std::size_t byteBudget_ = 0;
        std::size_t hitCount_ = 0;
        std::size_t missCount_ = 0;
    };

} // namespace us4::runtime::cache
