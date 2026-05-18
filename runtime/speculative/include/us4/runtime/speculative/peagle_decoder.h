#pragma once

#include "us4/runtime/speculative/speculative_engine.h"

#include <cstddef>
#include <vector>

namespace us4::runtime::speculative
{

    struct PEagleConfig
    {
        std::size_t draftDepth = 4;
        float acceptanceThreshold = 0.6F;
        bool useLayerPruning = true;
    };

    struct PEagleDecoderStats
    {
        std::size_t draftCount = 0;
        std::size_t acceptedTokens = 0;
        std::size_t rejectedTokens = 0;
        float averageAcceptanceRate = 0.0F;
    };

    class PEagleDecoder
    {
      public:
        void Configure(PEagleConfig config);
        [[nodiscard]] SpeculativeWindow Decode(const backends::TokenChunk& draft,
                                                const backends::TokenChunk& target);
        [[nodiscard]] PEagleDecoderStats Stats() const;
        void Reset();

      private:
        PEagleConfig config_{};
        SpeculativeEngine engine_{};
        std::size_t draftCount_ = 0;
        std::size_t acceptedTokens_ = 0;
        std::size_t rejectedTokens_ = 0;
    };

} // namespace us4::runtime::speculative
