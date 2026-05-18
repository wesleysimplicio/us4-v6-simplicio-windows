#include "us4/runtime/scheduler/continuous_batcher.h"

#include <algorithm>

namespace us4::runtime::scheduler
{

    void ContinuousBatcher::Configure(const std::size_t maxBatchSize, const std::size_t maxQueueDepth)
    {
        maxBatchSize_ = maxBatchSize;
        maxQueueDepth_ = maxQueueDepth;
    }

    bool ContinuousBatcher::Enqueue(BatchRequest request)
    {
        if (maxQueueDepth_ > 0 && queue_.size() >= maxQueueDepth_)
        {
            return false;
        }
        request.enqueueTick = tick_;
        const auto insertAt = std::find_if(queue_.begin(), queue_.end(), [&](const BatchRequest& existing) {
            return HasHigherPriority(request, existing);
        });
        queue_.insert(insertAt, std::move(request));
        enqueueCount_ += 1;
        if (queue_.size() > maxObservedDepth_)
        {
            maxObservedDepth_ = queue_.size();
        }
        return true;
    }

    BatchStep ContinuousBatcher::Step()
    {
        BatchStep step{};
        step.tick = tick_++;
        const auto capacity = (maxBatchSize_ == 0) ? queue_.size() : maxBatchSize_;
        const auto scheduled = std::min(capacity, queue_.size());
        step.scheduledCount = scheduled;
        step.requestIds.reserve(scheduled);
        for (std::size_t i = 0; i < scheduled; ++i)
        {
            auto request = std::move(queue_.front());
            queue_.pop_front();
            step.requestIds.push_back(request.requestId);
            inFlight_[request.requestId] = std::move(request);
        }
        stepCount_ += 1;
        return step;
    }

    void ContinuousBatcher::Complete(const std::string& requestId)
    {
        if (inFlight_.erase(requestId) > 0)
        {
            completeCount_ += 1;
        }
    }

    std::size_t ContinuousBatcher::QueueDepth() const
    {
        return queue_.size();
    }

    ContinuousBatcherStats ContinuousBatcher::Stats() const
    {
        ContinuousBatcherStats stats{};
        stats.enqueueCount = enqueueCount_;
        stats.completeCount = completeCount_;
        stats.stepCount = stepCount_;
        stats.maxQueueDepth = maxObservedDepth_;
        stats.currentQueueDepth = queue_.size();
        return stats;
    }

    bool ContinuousBatcher::HasHigherPriority(const BatchRequest& lhs, const BatchRequest& rhs)
    {
        if (lhs.priority != rhs.priority)
        {
            return lhs.priority > rhs.priority;
        }
        return lhs.enqueueTick < rhs.enqueueTick;
    }

} // namespace us4::runtime::scheduler
