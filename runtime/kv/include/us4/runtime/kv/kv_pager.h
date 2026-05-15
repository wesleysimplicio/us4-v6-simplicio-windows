#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace us4::runtime::kv
{

    struct KvPagerBudget
    {
        std::size_t deviceBytes = 0;
        std::size_t hostBytes = 0;
        std::size_t storageBytes = 0;
    };

    struct KvSegment
    {
        std::string sequenceId;
        std::size_t tokenCount = 0;
        std::size_t residentBytes = 0;
        std::string tier = "host";
        bool pinned = false;
    };

    struct KvPagerStats
    {
        std::size_t segmentCount = 0;
        std::size_t pinnedSegmentCount = 0;
        std::size_t residentBytes = 0;
        std::size_t deviceBytes = 0;
        std::size_t hostBytes = 0;
        std::size_t storageBytes = 0;
    };

    class KvPager
    {
      public:
        void ConfigureBudget(KvPagerBudget budget);
        [[nodiscard]] bool Pin(const KvSegment& segment);
        [[nodiscard]] bool Evict(const std::string& sequenceId, const std::string& targetTier);
        [[nodiscard]] std::optional<KvSegment> Find(const std::string& sequenceId) const;
        [[nodiscard]] KvPagerStats Stats() const;
        [[nodiscard]] std::vector<KvSegment> Snapshot() const;

      private:
        KvPagerBudget budget_;
        std::vector<KvSegment> segments_;
    };

} // namespace us4::runtime::kv
