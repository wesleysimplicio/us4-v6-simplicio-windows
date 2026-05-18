#include "us4/runtime/speculative/eagle3_decoder.h"

#include <algorithm>

namespace us4::runtime::speculative
{

    void Eagle3Decoder::Configure(Eagle3Config config)
    {
        config_ = config;
    }

    SpeculativeWindow Eagle3Decoder::Decode(const backends::TokenChunk& draft,
                                              const backends::TokenChunk& target)
    {
        const auto window = engine_.Verify(draft, target);
        draftCount_ += 1;
        acceptedTokens_ += window.acceptedTokens;
        rejectedTokens_ += window.rejectedTokens;
        maxTreeWidthObserved_ = std::max(maxTreeWidthObserved_, config_.treeWidth);
        return window;
    }

    Eagle3DecoderStats Eagle3Decoder::Stats() const
    {
        Eagle3DecoderStats stats{};
        stats.draftCount = draftCount_;
        stats.acceptedTokens = acceptedTokens_;
        stats.rejectedTokens = rejectedTokens_;
        stats.maxTreeWidthObserved = maxTreeWidthObserved_;
        const auto total = acceptedTokens_ + rejectedTokens_;
        if (total > 0)
        {
            const auto acceptanceRate = static_cast<float>(acceptedTokens_) / static_cast<float>(total);
            stats.averageAcceptanceRate = acceptanceRate;
            stats.estimatedSpeedup = 1.0F + (acceptanceRate * static_cast<float>(config_.treeDepth - 1));
        }
        return stats;
    }

    void Eagle3Decoder::Reset()
    {
        draftCount_ = 0;
        acceptedTokens_ = 0;
        rejectedTokens_ = 0;
        maxTreeWidthObserved_ = 0;
    }

} // namespace us4::runtime::speculative
