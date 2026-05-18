#include "us4/runtime/moe/speculative_prefetch.h"

#include <algorithm>

namespace us4::runtime::moe
{

    void SpeculativePrefetch::Configure(const std::size_t historyWindow, const std::size_t prefetchK)
    {
        historyWindow_ = historyWindow;
        prefetchK_ = prefetchK;
    }

    void SpeculativePrefetch::Observe(const std::size_t expertId)
    {
        history_.push_back(expertId);
        frequency_[expertId] += 1;
        observationCount_ += 1;
        while (historyWindow_ > 0 && history_.size() > historyWindow_)
        {
            const auto stale = history_.front();
            history_.pop_front();
            auto it = frequency_.find(stale);
            if (it != frequency_.end() && it->second > 0)
            {
                it->second -= 1;
                if (it->second == 0)
                {
                    frequency_.erase(it);
                }
            }
        }
    }

    std::vector<std::size_t> SpeculativePrefetch::PredictNext() const
    {
        std::vector<std::pair<std::size_t, std::size_t>> ranked(frequency_.begin(), frequency_.end());
        std::sort(ranked.begin(), ranked.end(),
                  [](const auto& lhs, const auto& rhs) { return lhs.second > rhs.second; });
        std::vector<std::size_t> top;
        top.reserve(std::min(prefetchK_, ranked.size()));
        for (std::size_t i = 0; i < prefetchK_ && i < ranked.size(); ++i)
        {
            top.push_back(ranked[i].first);
        }
        return top;
    }

    void SpeculativePrefetch::RecordPrefetchOutcome(const std::size_t expertId, const bool wasUsed)
    {
        predictionCount_ += 1;
        if (wasUsed)
        {
            hitCount_ += 1;
        }
        else
        {
            missCount_ += 1;
        }
        (void) expertId;
    }

    SpeculativePrefetchStats SpeculativePrefetch::Stats() const
    {
        SpeculativePrefetchStats stats{};
        stats.observationCount = observationCount_;
        stats.predictionCount = predictionCount_;
        stats.prefetchHitCount = hitCount_;
        stats.prefetchMissCount = missCount_;
        const auto total = hitCount_ + missCount_;
        if (total > 0)
        {
            stats.hitRatio = static_cast<float>(hitCount_) / static_cast<float>(total);
        }
        return stats;
    }

    void SpeculativePrefetch::Reset()
    {
        history_.clear();
        frequency_.clear();
        observationCount_ = 0;
        predictionCount_ = 0;
        hitCount_ = 0;
        missCount_ = 0;
    }

} // namespace us4::runtime::moe
