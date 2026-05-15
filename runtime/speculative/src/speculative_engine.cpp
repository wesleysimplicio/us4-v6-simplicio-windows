#include "us4/runtime/speculative/speculative_engine.h"

namespace us4::runtime::speculative
{

    SpeculativeWindow SpeculativeEngine::Verify(const backends::TokenChunk& draft,
                                                const backends::TokenChunk& target) const
    {
        const std::size_t accepted = draft.tokens == target.tokens ? draft.tokens.size() : 0U;
        const std::size_t rejected = draft.tokens.size() - accepted;
        const float acceptanceRate =
            draft.tokens.empty()
                ? 0.0F
                : static_cast<float>(accepted) / static_cast<float>(draft.tokens.size());
        return SpeculativeWindow{
            .draftTokens = draft.tokens.size(),
            .acceptedTokens = accepted,
            .rejectedTokens = rejected,
            .acceptanceRate = acceptanceRate,
            .isLossless = accepted == target.tokens.size(),
        };
    }

    bool SpeculativeEngine::ShouldCommit(const SpeculativeWindow& window) const
    {
        return window.isLossless || window.acceptanceRate >= 0.5F;
    }

} // namespace us4::runtime::speculative
