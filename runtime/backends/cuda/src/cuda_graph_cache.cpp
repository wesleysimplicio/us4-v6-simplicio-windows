#include "us4/runtime/backends/cuda/cuda_graph_cache.h"

#include <algorithm>
#include <functional>
#include <utility>

namespace us4::runtime::backends::cuda
{

    std::size_t HashGraphSignature(const GraphSignature& sig)
    {
        std::size_t hash = std::hash<std::string>{}(sig.adapterId);
        hash ^= std::hash<std::string>{}(sig.stage) << 1;
        hash ^= std::hash<std::size_t>{}(sig.batchSize) << 2;
        hash ^= std::hash<std::size_t>{}(sig.sequenceLength) << 3;
        hash ^= std::hash<std::size_t>{}(sig.kvLength) << 4;
        return hash;
    }

    void CudaGraphCache::Configure(const std::size_t maxEntries)
    {
        maxEntries_ = maxEntries;
    }

    bool CudaGraphCache::Lookup(const GraphSignature& signature)
    {
        const auto hash = (signature.hash == 0) ? HashGraphSignature(signature) : signature.hash;
        const auto it = entries_.find(hash);
        ++tick_;
        if (it == entries_.end())
        {
            missCount_ += 1;
            return false;
        }
        it->second.launchCount += 1;
        it->second.lastUseTick = tick_;
        hitCount_ += 1;
        reuseCount_ += 1;
        return true;
    }

    void CudaGraphCache::Record(GraphSignature signature)
    {
        if (signature.hash == 0)
        {
            signature.hash = HashGraphSignature(signature);
        }
        ++tick_;
        if (maxEntries_ > 0 && entries_.size() >= maxEntries_)
        {
            auto oldest = entries_.begin();
            for (auto it = entries_.begin(); it != entries_.end(); ++it)
            {
                if (it->second.lastUseTick < oldest->second.lastUseTick)
                {
                    oldest = it;
                }
            }
            if (oldest != entries_.end())
            {
                entries_.erase(oldest);
                evictionCount_ += 1;
            }
        }
        GraphEntry entry{};
        entry.signature = signature;
        entry.launchCount = 1;
        entry.lastUseTick = tick_;
        entries_[signature.hash] = std::move(entry);
    }

    bool CudaGraphCache::Evict(const std::size_t hash)
    {
        if (entries_.erase(hash) > 0)
        {
            evictionCount_ += 1;
            return true;
        }
        return false;
    }

    GraphCacheStats CudaGraphCache::Stats() const
    {
        GraphCacheStats stats{};
        stats.entryCount = entries_.size();
        stats.hitCount = hitCount_;
        stats.missCount = missCount_;
        stats.reuseCount = reuseCount_;
        stats.evictionCount = evictionCount_;
        const auto total = hitCount_ + missCount_;
        if (total > 0)
        {
            stats.hitRatio = static_cast<float>(hitCount_) / static_cast<float>(total);
        }
        return stats;
    }

    std::vector<GraphEntry> CudaGraphCache::Snapshot() const
    {
        std::vector<GraphEntry> snapshot;
        snapshot.reserve(entries_.size());
        for (const auto& [_, entry] : entries_)
        {
            snapshot.push_back(entry);
        }
        return snapshot;
    }

} // namespace us4::runtime::backends::cuda
