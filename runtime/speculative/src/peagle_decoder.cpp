#include "us4/runtime/speculative/peagle_decoder.h"

namespace us4::runtime::speculative
{

    void PEagleDecoder::Configure(PEagleConfig config)
    {
        config_ = config;
    }

    SpeculativeWindow PEagleDecoder::Decode(const backends::TokenChunk& draft,
                                              const backends::TokenChunk& target)
    {
        const auto window = engine_.Verify(draft, target);
        draftCount_ += 1;
        acceptedTokens_ += window.acceptedTokens;
        rejectedTokens_ += window.rejectedTokens;
        return window;
    }

    PEagleDecoderStats PEagleDecoder::Stats() const
    {
        PEagleDecoderStats stats{};
        stats.draftCount = draftCount_;
        stats.acceptedTokens = acceptedTokens_;
        stats.rejectedTokens = rejectedTokens_;
        const auto total = acceptedTokens_ + rejectedTokens_;
        if (total > 0)
        {
            stats.averageAcceptanceRate = static_cast<float>(acceptedTokens_) / static_cast<float>(total);
        }
        return stats;
    }

    void PEagleDecoder::Reset()
    {
        draftCount_ = 0;
        acceptedTokens_ = 0;
        rejectedTokens_ = 0;
    }

} // namespace us4::runtime::speculative
