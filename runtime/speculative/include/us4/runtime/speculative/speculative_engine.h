#pragma once

#include "us4/runtime/backends/runtime_types.h"

#include <cstddef>

namespace us4::runtime::speculative
{

    struct SpeculativeWindow
    {
        std::size_t draftTokens = 0;
        std::size_t acceptedTokens = 0;
        std::size_t rejectedTokens = 0;
        float acceptanceRate = 0.0F;
        bool isLossless = true;
    };

    class SpeculativeEngine
    {
      public:
        [[nodiscard]] SpeculativeWindow Verify(const backends::TokenChunk& draft,
                                               const backends::TokenChunk& target) const;
        [[nodiscard]] bool ShouldCommit(const SpeculativeWindow& window) const;
    };

} // namespace us4::runtime::speculative
