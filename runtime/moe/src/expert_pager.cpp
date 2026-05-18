#include "us4/runtime/moe/expert_pager.h"

#include <algorithm>
#include <array>

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
            const auto payload = BuildPayload(expertId, residentBytes);
            entries_.push_back(ExpertEntry{
                .expertId = expertId,
                .residency = preferredResidency,
                .residentBytes = residentBytes,
                .touchCount = 1U,
                .lastTouchTick = nextTouchTick_++,
                .offloaded = preferredResidency == ExpertResidency::kCold,
                .reloadCount = 0U,
                .payloadChecksum = Checksum(payload),
            });
            if (preferredResidency == ExpertResidency::kCold)
            {
                coldStore_[expertId] = ColdExpertBacking{
                    .payload = payload,
                    .checksum = entries_.back().payloadChecksum,
                };
                ++coldOffloadCount_;
            }
        }
        else
        {
            ExpertEntry& entry = entries_[*index];
            if (entry.offloaded && preferredResidency != ExpertResidency::kCold &&
                !RestoreColdEntry(*index, preferredResidency))
            {
                return false;
            }

            entry.residentBytes = residentBytes;
            entry.residency = preferredResidency;
            entry.touchCount += 1U;
            entry.lastTouchTick = nextTouchTick_++;
            const auto payload = BuildPayload(expertId, residentBytes);
            entry.payloadChecksum = Checksum(payload);
            if (preferredResidency == ExpertResidency::kCold)
            {
                coldStore_[expertId] = ColdExpertBacking{
                    .payload = payload,
                    .checksum = entry.payloadChecksum,
                };
                entry.offloaded = true;
                ++coldOffloadCount_;
            }
            else
            {
                entry.offloaded = false;
                coldStore_.erase(expertId);
            }
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

    bool ExpertPager::Reload(const std::size_t expertId, const ExpertResidency targetResidency)
    {
        const auto index = FindIndex(expertId);
        if (!index.has_value())
        {
            return false;
        }

        if (!entries_[*index].offloaded)
        {
            return MoveToResidency(*index, targetResidency);
        }

        if (!RestoreColdEntry(*index, targetResidency))
        {
            return false;
        }

        static_cast<void>(Rebalance());
        return true;
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
            .coldOffloadCount = coldOffloadCount_,
            .reloadCount = reloadCount_,
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

        if (entry.offloaded && targetResidency != ExpertResidency::kCold)
        {
            const bool restored = RestoreColdEntry(index, targetResidency);
            if (restored)
            {
                ++promotionCount_;
            }
            return restored;
        }

        entry.residency = targetResidency;
        entry.lastTouchTick = nextTouchTick_++;
        if (targetResidency == ExpertResidency::kCold)
        {
            return PersistColdEntry(index);
        }

        coldStore_.erase(entry.expertId);
        entry.offloaded = false;
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
            static_cast<void>(PersistColdEntry(*victim));
            ++evictionCount_;
            ++moves;
        }

        return moves;
    }

    bool ExpertPager::PersistColdEntry(const std::size_t index)
    {
        if (index >= entries_.size())
        {
            return false;
        }

        auto& entry = entries_[index];
        const auto payload = BuildPayload(entry.expertId, entry.residentBytes);
        entry.payloadChecksum = Checksum(payload);
        entry.offloaded = true;
        coldStore_[entry.expertId] = ColdExpertBacking{
            .payload = payload,
            .checksum = entry.payloadChecksum,
        };
        ++coldOffloadCount_;
        return true;
    }

    bool ExpertPager::RestoreColdEntry(const std::size_t index, const ExpertResidency targetResidency)
    {
        if (index >= entries_.size())
        {
            return false;
        }

        auto& entry = entries_[index];
        const auto it = coldStore_.find(entry.expertId);
        if (it == coldStore_.end())
        {
            return false;
        }
        if (Checksum(it->second.payload) != it->second.checksum)
        {
            return false;
        }

        entry.residency = targetResidency;
        entry.offloaded = false;
        entry.reloadCount += 1U;
        entry.lastTouchTick = nextTouchTick_++;
        entry.payloadChecksum = it->second.checksum;
        coldStore_.erase(it);
        ++reloadCount_;
        return true;
    }

    std::vector<std::uint8_t> ExpertPager::BuildPayload(const std::size_t expertId,
                                                        const std::size_t residentBytes)
    {
        const std::array<std::uint64_t, 2> payloadWords{
            static_cast<std::uint64_t>(expertId),
            static_cast<std::uint64_t>(residentBytes),
        };
        std::vector<std::uint8_t> payload(sizeof(payloadWords));
        const auto* source = reinterpret_cast<const std::uint8_t*>(payloadWords.data());
        std::copy(source, source + payload.size(), payload.begin());
        return payload;
    }

    std::uint64_t ExpertPager::Checksum(const std::vector<std::uint8_t>& payload)
    {
        std::uint64_t fnv = 14695981039346656037ULL;
        for (const auto byte : payload)
        {
            fnv ^= byte;
            fnv *= 1099511628211ULL;
        }
        return fnv;
    }

} // namespace us4::runtime::moe
