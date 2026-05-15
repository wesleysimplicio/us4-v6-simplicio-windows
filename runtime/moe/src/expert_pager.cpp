#include "us4/runtime/moe/expert_pager.h"

#include <algorithm>

namespace us4::runtime::moe
{

    void ExpertPager::ConfigureBudget(const ExpertPagerBudget budget)
    {
        budget_ = budget;
    }

    bool ExpertPager::Touch(const std::size_t expertId, const std::size_t residentBytes,
                            const ExpertResidency preferredResidency)
    {
        const auto index = FindIndex(expertId);
        if (!index.has_value())
        {
            entries_.push_back(ExpertEntry{
                .expertId = expertId,
                .residency = preferredResidency,
                .residentBytes = residentBytes,
                .touchCount = 1U,
                .lastTouchTick = nextTouchTick_++,
            });
        }
        else
        {
            ExpertEntry& entry = entries_[*index];
            entry.residentBytes = residentBytes;
            entry.residency = preferredResidency;
            entry.touchCount += 1U;
            entry.lastTouchTick = nextTouchTick_++;
        }

        static_cast<void>(Rebalance());
        return true;
    }

    bool ExpertPager::Promote(const std::size_t expertId, const ExpertResidency targetResidency)
    {
        const auto index = FindIndex(expertId);
        if (!index.has_value())
        {
            return false;
        }

        return MoveToResidency(*index, targetResidency);
    }

    std::optional<ExpertEntry> ExpertPager::Find(const std::size_t expertId) const
    {
        const auto index = FindIndex(expertId);
        if (!index.has_value())
        {
            return std::nullopt;
        }
        return entries_[*index];
    }

    ExpertPagerStats ExpertPager::Stats() const
    {
        ExpertPagerStats stats{
            .promotionCount = promotionCount_,
            .evictionCount = evictionCount_,
        };

        for (const auto& entry : entries_)
        {
            switch (entry.residency)
            {
            case ExpertResidency::kHot:
                ++stats.hotExperts;
                stats.hotBytes += entry.residentBytes;
                break;
            case ExpertResidency::kWarm:
                ++stats.warmExperts;
                stats.warmBytes += entry.residentBytes;
                break;
            case ExpertResidency::kCold:
                ++stats.coldExperts;
                stats.coldBytes += entry.residentBytes;
                break;
            }
        }

        return stats;
    }

    std::vector<ExpertEntry> ExpertPager::Snapshot() const
    {
        return entries_;
    }

    std::size_t ExpertPager::ResidencyBytes(const ExpertResidency residency) const
    {
        std::size_t bytes = 0;
        for (const auto& entry : entries_)
        {
            if (entry.residency == residency)
            {
                bytes += entry.residentBytes;
            }
        }
        return bytes;
    }

    std::size_t ExpertPager::ResidencyBudget(const ExpertResidency residency) const
    {
        switch (residency)
        {
        case ExpertResidency::kHot:
            return budget_.hotBytes;
        case ExpertResidency::kWarm:
            return budget_.warmBytes;
        case ExpertResidency::kCold:
            return budget_.coldBytes;
        }
        return 0;
    }

    std::optional<std::size_t> ExpertPager::FindIndex(const std::size_t expertId) const
    {
        const auto it = std::find_if(entries_.begin(), entries_.end(),
                                     [expertId](const ExpertEntry& entry)
                                     { return entry.expertId == expertId; });
        if (it == entries_.end())
        {
            return std::nullopt;
        }
        return static_cast<std::size_t>(std::distance(entries_.begin(), it));
    }

    bool ExpertPager::MoveToResidency(const std::size_t index, const ExpertResidency targetResidency)
    {
        if (index >= entries_.size())
        {
            return false;
        }

        ExpertEntry& entry = entries_[index];
        if (entry.residency == targetResidency)
        {
            return true;
        }

        entry.residency = targetResidency;
        entry.lastTouchTick = nextTouchTick_++;
        ++promotionCount_;
        return true;
    }

    std::optional<std::size_t> ExpertPager::SelectVictim(const ExpertResidency residency) const
    {
        std::optional<std::size_t> victim;
        for (std::size_t index = 0; index < entries_.size(); ++index)
        {
            const auto& entry = entries_[index];
            if (entry.residency != residency)
            {
                continue;
            }

            if (!victim.has_value())
            {
                victim = index;
                continue;
            }

            const auto& current = entries_[*victim];
            if (entry.touchCount < current.touchCount ||
                (entry.touchCount == current.touchCount && entry.lastTouchTick < current.lastTouchTick))
            {
                victim = index;
            }
        }
        return victim;
    }

    std::size_t ExpertPager::Rebalance()
    {
        std::size_t moves = 0;
        while (ResidencyBytes(ExpertResidency::kHot) > ResidencyBudget(ExpertResidency::kHot))
        {
            const auto victim = SelectVictim(ExpertResidency::kHot);
            if (!victim.has_value())
            {
                break;
            }
            entries_[*victim].residency = ExpertResidency::kWarm;
            ++evictionCount_;
            ++moves;
        }

        while (ResidencyBytes(ExpertResidency::kWarm) > ResidencyBudget(ExpertResidency::kWarm))
        {
            const auto victim = SelectVictim(ExpertResidency::kWarm);
            if (!victim.has_value())
            {
                break;
            }
            entries_[*victim].residency = ExpertResidency::kCold;
            ++evictionCount_;
            ++moves;
        }

        return moves;
    }

} // namespace us4::runtime::moe
