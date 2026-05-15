#pragma once

#include <cstddef>
#include <optional>
#include <vector>

namespace us4::runtime::moe
{

    enum class ExpertResidency
    {
        kHot,
        kWarm,
        kCold,
    };

    struct ExpertPagerBudget
    {
        std::size_t hotBytes = 0;
        std::size_t warmBytes = 0;
        std::size_t coldBytes = 0;
    };

    struct ExpertEntry
    {
        std::size_t expertId = 0;
        ExpertResidency residency = ExpertResidency::kHot;
        std::size_t residentBytes = 0;
        std::size_t touchCount = 0;
        std::size_t lastTouchTick = 0;
    };

    struct ExpertPagerStats
    {
        std::size_t hotExperts = 0;
        std::size_t warmExperts = 0;
        std::size_t coldExperts = 0;
        std::size_t hotBytes = 0;
        std::size_t warmBytes = 0;
        std::size_t coldBytes = 0;
        std::size_t promotionCount = 0;
        std::size_t evictionCount = 0;
    };

    class ExpertPager
    {
      public:
        void ConfigureBudget(ExpertPagerBudget budget);
        [[nodiscard]] bool Touch(std::size_t expertId, std::size_t residentBytes,
                                 ExpertResidency preferredResidency);
        [[nodiscard]] bool Promote(std::size_t expertId, ExpertResidency targetResidency);
        [[nodiscard]] std::optional<ExpertEntry> Find(std::size_t expertId) const;
        [[nodiscard]] ExpertPagerStats Stats() const;
        [[nodiscard]] std::vector<ExpertEntry> Snapshot() const;

      private:
        [[nodiscard]] std::size_t ResidencyBytes(ExpertResidency residency) const;
        [[nodiscard]] std::size_t ResidencyBudget(ExpertResidency residency) const;
        [[nodiscard]] std::optional<std::size_t> FindIndex(std::size_t expertId) const;
        [[nodiscard]] bool MoveToResidency(std::size_t index, ExpertResidency targetResidency);
        [[nodiscard]] std::optional<std::size_t> SelectVictim(ExpertResidency residency) const;
        [[nodiscard]] std::size_t Rebalance();

        ExpertPagerBudget budget_;
        std::vector<ExpertEntry> entries_;
        std::size_t nextTouchTick_ = 1;
        std::size_t promotionCount_ = 0;
        std::size_t evictionCount_ = 0;
    };

} // namespace us4::runtime::moe
