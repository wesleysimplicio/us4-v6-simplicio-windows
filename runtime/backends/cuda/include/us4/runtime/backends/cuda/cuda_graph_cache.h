#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace us4::runtime::backends::cuda
{

    struct GraphSignature
    {
        std::string adapterId;
        std::string stage;
        std::size_t batchSize = 0;
        std::size_t sequenceLength = 0;
        std::size_t kvLength = 0;
        std::size_t hash = 0;
    };

    [[nodiscard]] std::size_t HashGraphSignature(const GraphSignature& sig);

    struct GraphEntry
    {
        GraphSignature signature;
        std::size_t launchCount = 0;
        std::size_t lastUseTick = 0;
    };

    struct GraphCacheStats
    {
        std::size_t entryCount = 0;
        std::size_t hitCount = 0;
        std::size_t missCount = 0;
        std::size_t reuseCount = 0;
        std::size_t evictionCount = 0;
        float hitRatio = 0.0F;
    };

    class CudaGraphCache
    {
      public:
        void Configure(std::size_t maxEntries);
        [[nodiscard]] bool Lookup(const GraphSignature& signature);
        void Record(GraphSignature signature);
        [[nodiscard]] bool Evict(std::size_t hash);
        [[nodiscard]] GraphCacheStats Stats() const;
        [[nodiscard]] std::vector<GraphEntry> Snapshot() const;

      private:
        std::size_t maxEntries_ = 0;
        std::size_t tick_ = 0;
        std::unordered_map<std::size_t, GraphEntry> entries_;
        std::size_t hitCount_ = 0;
        std::size_t missCount_ = 0;
        std::size_t reuseCount_ = 0;
        std::size_t evictionCount_ = 0;
    };

} // namespace us4::runtime::backends::cuda
