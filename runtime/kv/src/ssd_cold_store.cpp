#include "us4/runtime/kv/ssd_cold_store.h"

#include <utility>

namespace us4::runtime::kv
{

    void SsdColdStore::ConfigureRoot(std::filesystem::path rootDir)
    {
        rootDir_ = std::move(rootDir);
    }

    bool SsdColdStore::Write(const std::string& sequenceId, std::vector<std::uint8_t> payload)
    {
        SsdColdStoreEntry entry{};
        entry.sequenceId = sequenceId;
        entry.backingFile = rootDir_ / (sequenceId + ".kv");
        entry.offsetBytes = 0;
        entry.lengthBytes = payload.size();
        entry.checksum = Checksum(payload);
        entry.flushed = false;
        entries_[sequenceId] = std::move(entry);
        buffer_[sequenceId] = std::move(payload);
        writeCount_ += 1;
        return true;
    }

    std::optional<std::vector<std::uint8_t>> SsdColdStore::Read(const std::string& sequenceId)
    {
        const auto it = buffer_.find(sequenceId);
        if (it == buffer_.end())
        {
            return std::nullopt;
        }
        readCount_ += 1;
        return it->second;
    }

    bool SsdColdStore::Flush(const std::string& sequenceId)
    {
        const auto it = entries_.find(sequenceId);
        if (it == entries_.end())
        {
            return false;
        }
        it->second.flushed = true;
        flushCount_ += 1;
        return true;
    }

    std::size_t SsdColdStore::FlushAll()
    {
        std::size_t flushed = 0;
        for (auto& [_, entry] : entries_)
        {
            if (!entry.flushed)
            {
                entry.flushed = true;
                flushed += 1;
                flushCount_ += 1;
            }
        }
        return flushed;
    }

    bool SsdColdStore::Evict(const std::string& sequenceId)
    {
        const auto removed = entries_.erase(sequenceId);
        buffer_.erase(sequenceId);
        if (removed > 0)
        {
            evictionCount_ += 1;
            return true;
        }
        return false;
    }

    std::optional<SsdColdStoreEntry> SsdColdStore::Find(const std::string& sequenceId) const
    {
        const auto it = entries_.find(sequenceId);
        if (it == entries_.end())
        {
            return std::nullopt;
        }
        return it->second;
    }

    SsdColdStoreStats SsdColdStore::Stats() const
    {
        SsdColdStoreStats stats{};
        stats.entryCount = entries_.size();
        stats.writeCount = writeCount_;
        stats.readCount = readCount_;
        stats.flushCount = flushCount_;
        stats.evictionCount = evictionCount_;
        for (const auto& [_, entry] : entries_)
        {
            stats.residentBytes += entry.lengthBytes;
        }
        return stats;
    }

    std::vector<SsdColdStoreEntry> SsdColdStore::Snapshot() const
    {
        std::vector<SsdColdStoreEntry> snapshot;
        snapshot.reserve(entries_.size());
        for (const auto& [_, entry] : entries_)
        {
            snapshot.push_back(entry);
        }
        return snapshot;
    }

    std::uint64_t SsdColdStore::Checksum(const std::vector<std::uint8_t>& payload)
    {
        std::uint64_t fnv = 14695981039346656037ULL;
        for (const auto byte : payload)
        {
            fnv ^= byte;
            fnv *= 1099511628211ULL;
        }
        return fnv;
    }

} // namespace us4::runtime::kv
