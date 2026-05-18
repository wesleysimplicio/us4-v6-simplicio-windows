#pragma once

#include "us4/runtime/speculative/speculative_engine.h"

#include <cstddef>
#include <vector>

namespace us4::runtime::speculative
{

    struct Eagle3Config
    {
        std::size_t treeDepth = 5;
        std::size_t treeWidth = 4;
        float acceptanceThreshold = 0.7F;
        bool useTreeAttention = true;
    };

    struct Eagle3DecoderStats
    {
        std::size_t draftCount = 0;
        std::size_t acceptedTokens = 0;
        std::size_t rejectedTokens = 0;
        std::size_t maxTreeWidthObserved = 0;
        float averageAcceptanceRate = 0.0F;
        float estimatedSpeedup = 1.0F;
    };

    class Eagle3Decoder
    {
      public:
        void Configure(Eagle3Config config);
        [[nodiscard]] SpeculativeWindow Decode(const backends::TokenChunk& draft,
                                                const backends::TokenChunk& target);
        [[nodiscard]] Eagle3DecoderStats Stats() const;
        void Reset();

      private:
        Eagle3Config config_{};
        SpeculativeEngine engine_{};
        std::size_t draftCount_ = 0;
        std::size_t acceptedTokens_ = 0;
        std::size_t rejectedTokens_ = 0;
        std::size_t maxTreeWidthObserved_ = 0;
    };

} // namespace us4::runtime::speculative
