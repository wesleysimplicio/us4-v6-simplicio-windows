#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace us4::runtime::scheduler
{

    struct SessionRecord
    {
        std::string sessionId;
        std::string kvNamespace;
        std::size_t promptTokens = 0;
        std::size_t generatedTokens = 0;
        std::size_t kvBytes = 0;
        bool active = true;
    };

    struct SessionPoolStats
    {
        std::size_t totalSessions = 0;
        std::size_t activeSessions = 0;
        std::size_t totalKvBytes = 0;
        std::size_t evictionCount = 0;
    };

    class SessionPool
    {
      public:
        void Configure(std::size_t maxConcurrentSessions, std::size_t kvByteBudget);
        [[nodiscard]] std::optional<std::string> Acquire(const std::string& sessionId);
        [[nodiscard]] bool Release(const std::string& sessionId);
        [[nodiscard]] bool Touch(const std::string& sessionId, std::size_t generatedTokens,
                                  std::size_t kvBytes);
        [[nodiscard]] std::optional<SessionRecord> Find(const std::string& sessionId) const;
        [[nodiscard]] SessionPoolStats Stats() const;
        [[nodiscard]] std::vector<SessionRecord> Snapshot() const;

      private:
        std::size_t maxConcurrentSessions_ = 0;
        std::size_t kvByteBudget_ = 0;
        std::unordered_map<std::string, SessionRecord> sessions_;
        std::size_t evictionCount_ = 0;
    };

} // namespace us4::runtime::scheduler
