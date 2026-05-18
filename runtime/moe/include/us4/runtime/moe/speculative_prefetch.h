#pragma once

#include <cstddef>
#include <deque>
#include <optional>
#include <unordered_map>
#include <vector>

namespace us4::runtime::moe
{

    struct SpeculativePrefetchStats
    {
        std::size_t observationCount = 0;
        std::size_t predictionCount = 0;
        std::size_t prefetchHitCount = 0;
        std::size_t prefetchMissCount = 0;
        float hitRatio = 0.0F;
    };

    class SpeculativePrefetch
    {
      public:
        void Configure(std::size_t historyWindow, std::size_t prefetchK);
        void Observe(std::size_t expertId);
        [[nodiscard]] std::vector<std::size_t> PredictNext() const;
        void RecordPrefetchOutcome(std::size_t expertId, bool wasUsed);
        [[nodiscard]] SpeculativePrefetchStats Stats() const;
        void Reset();

      private:
        std::size_t historyWindow_ = 0;
        std::size_t prefetchK_ = 0;
        std::deque<std::size_t> history_;
        std::unordered_map<std::size_t, std::size_t> frequency_;
        std::size_t observationCount_ = 0;
        std::size_t predictionCount_ = 0;
        std::size_t hitCount_ = 0;
        std::size_t missCount_ = 0;
    };

} // namespace us4::runtime::moe
