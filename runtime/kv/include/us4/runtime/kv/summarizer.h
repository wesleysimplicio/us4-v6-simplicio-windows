#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace us4::runtime::kv
{

    struct SummaryWindow
    {
        std::size_t headTokens = 8;
        std::size_t tailTokens = 8;
    };

    struct SummaryResult
    {
        std::vector<std::int32_t> retainedTokens;
        std::size_t droppedTokens = 0;
        std::size_t estimatedBytes = 0;
    };

    class Summarizer
    {
      public:
        [[nodiscard]] static SummaryResult Compact(const std::vector<std::int32_t>& tokens,
                                                   SummaryWindow window = {});
    };

} // namespace us4::runtime::kv
