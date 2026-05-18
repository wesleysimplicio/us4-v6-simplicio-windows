#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace us4::runtime::cache
{

    struct SparsityCacheEntry
    {
        std::string key;
        std::size_t activeChannels = 0;
        std::size_t totalChannels = 0;
        std::size_t residentBytes = 0;
        std::size_t hitCount = 0;
        std::size_t missCount = 0;
    };

    struct SparsityCacheStats
    {
        std::size_t entryCount = 0;
        std::size_t hitCount = 0;
        std::size_t missCount = 0;
        std::size_t residentBytes = 0;
        float averageSparsity = 0.0F;
    };

    class SparsityAwareCache
    {
      public:
        void Configure(std::size_t channelBudget, float sparsityThreshold);
        void Upsert(SparsityCacheEntry entry);
        [[nodiscard]] std::optional<SparsityCacheEntry> TryGet(const std::string& key);
        [[nodiscard]] bool Contains(const std::string& key) const;
        [[nodiscard]] SparsityCacheStats Stats() const;
        [[nodiscard]] std::vector<SparsityCacheEntry> Snapshot() const;
        [[nodiscard]] std::size_t EvictBelowThreshold();

      private:
        [[nodiscard]] float SparsityRatio(const SparsityCacheEntry& entry) const;

        std::unordered_map<std::string, SparsityCacheEntry> entries_;
        std::size_t channelBudget_ = 0;
        float sparsityThreshold_ = 0.0F;
        std::size_t hitCount_ = 0;
        std::size_t missCount_ = 0;
    };

} // namespace us4::runtime::cache
