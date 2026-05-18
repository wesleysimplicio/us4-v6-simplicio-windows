#include "us4/runtime/scheduler/session_pool.h"

namespace us4::runtime::scheduler
{

    void SessionPool::Configure(const std::size_t maxConcurrentSessions, const std::size_t kvByteBudget)
    {
        maxConcurrentSessions_ = maxConcurrentSessions;
        kvByteBudget_ = kvByteBudget;
    }

    std::optional<std::string> SessionPool::Acquire(const std::string& sessionId)
    {
        if (sessions_.find(sessionId) != sessions_.end())
        {
            return sessionId + ":kv";
        }
        if (maxConcurrentSessions_ > 0 && sessions_.size() >= maxConcurrentSessions_)
        {
            return std::nullopt;
        }
        SessionRecord record{};
        record.sessionId = sessionId;
        record.kvNamespace = sessionId + ":kv";
        record.active = true;
        sessions_[sessionId] = record;
        return record.kvNamespace;
    }

    bool SessionPool::Release(const std::string& sessionId)
    {
        const auto it = sessions_.find(sessionId);
        if (it == sessions_.end())
        {
            return false;
        }
        it->second.active = false;
        sessions_.erase(it);
        evictionCount_ += 1;
        return true;
    }

    bool SessionPool::Touch(const std::string& sessionId, const std::size_t generatedTokens,
                             const std::size_t kvBytes)
    {
        const auto it = sessions_.find(sessionId);
        if (it == sessions_.end())
        {
            return false;
        }
        it->second.generatedTokens += generatedTokens;
        it->second.kvBytes = kvBytes;
        return true;
    }

    std::optional<SessionRecord> SessionPool::Find(const std::string& sessionId) const
    {
        const auto it = sessions_.find(sessionId);
        if (it == sessions_.end())
        {
            return std::nullopt;
        }
        return it->second;
    }

    SessionPoolStats SessionPool::Stats() const
    {
        SessionPoolStats stats{};
        stats.totalSessions = sessions_.size();
        stats.evictionCount = evictionCount_;
        for (const auto& [_, record] : sessions_)
        {
            if (record.active)
            {
                stats.activeSessions += 1;
            }
            stats.totalKvBytes += record.kvBytes;
        }
        return stats;
    }

    std::vector<SessionRecord> SessionPool::Snapshot() const
    {
        std::vector<SessionRecord> snapshot;
        snapshot.reserve(sessions_.size());
        for (const auto& [_, record] : sessions_)
        {
            snapshot.push_back(record);
        }
        return snapshot;
    }

} // namespace us4::runtime::scheduler
