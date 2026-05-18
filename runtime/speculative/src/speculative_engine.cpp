#include "us4/runtime/speculative/speculative_engine.h"

#include <algorithm>

namespace us4::runtime::speculative
{

    SpeculativeWindow SpeculativeEngine::Verify(const backends::TokenChunk& draft,
                                                const backends::TokenChunk& target) const
    {
        const std::size_t comparedTokens =
            std::min(draft.tokens.size(), target.tokens.size());
        std::size_t accepted = 0U;
        while (accepted < comparedTokens && draft.tokens[accepted] == target.tokens[accepted])
        {
            ++accepted;
        }
        const std::size_t rejected =
            draft.tokens.size() > accepted ? draft.tokens.size() - accepted : 0U;
        const float acceptanceRate =
            draft.tokens.empty()
                ? 0.0F
                : static_cast<float>(accepted) / static_cast<float>(draft.tokens.size());
        return SpeculativeWindow{
            .draftTokens = draft.tokens.size(),
            .acceptedTokens = accepted,
            .rejectedTokens = rejected,
            .acceptanceRate = acceptanceRate,
            .isLossless = accepted == draft.tokens.size() && draft.tokens.size() == target.tokens.size(),
        };
    }

    bool SpeculativeEngine::ShouldCommit(const SpeculativeWindow& window) const
    {
        return window.isLossless || window.acceptanceRate >= 0.5F;
    }

} // namespace us4::runtime::speculative
