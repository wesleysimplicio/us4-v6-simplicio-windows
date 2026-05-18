#pragma once

#include <cstddef>
#include <deque>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace us4::runtime::scheduler
{

    struct BatchRequest
    {
        std::string sessionId;
        std::string requestId;
        std::size_t promptTokens = 0;
        std::size_t maxNewTokens = 0;
        std::size_t priority = 0;
        std::size_t enqueueTick = 0;
    };

    struct BatchStep
    {
        std::size_t tick = 0;
        std::size_t scheduledCount = 0;
        std::vector<std::string> requestIds;
    };

    struct ContinuousBatcherStats
    {
        std::size_t enqueueCount = 0;
        std::size_t completeCount = 0;
        std::size_t stepCount = 0;
        std::size_t maxQueueDepth = 0;
        std::size_t currentQueueDepth = 0;
    };

    class ContinuousBatcher
    {
      public:
        void Configure(std::size_t maxBatchSize, std::size_t maxQueueDepth);
        [[nodiscard]] bool Enqueue(BatchRequest request);
        [[nodiscard]] BatchStep Step();
        void Complete(const std::string& requestId);
        [[nodiscard]] std::size_t QueueDepth() const;
        [[nodiscard]] ContinuousBatcherStats Stats() const;

      private:
        [[nodiscard]] static bool HasHigherPriority(const BatchRequest& lhs, const BatchRequest& rhs);

        std::size_t maxBatchSize_ = 0;
        std::size_t maxQueueDepth_ = 0;
        std::size_t tick_ = 0;
        std::deque<BatchRequest> queue_;
        std::unordered_map<std::string, BatchRequest> inFlight_;
        std::size_t enqueueCount_ = 0;
        std::size_t completeCount_ = 0;
        std::size_t stepCount_ = 0;
        std::size_t maxObservedDepth_ = 0;
    };

} // namespace us4::runtime::scheduler
