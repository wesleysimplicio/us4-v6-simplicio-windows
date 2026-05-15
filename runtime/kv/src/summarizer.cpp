#include "us4/runtime/kv/summarizer.h"

#include <algorithm>

namespace us4::runtime::kv
{

    SummaryResult Summarizer::Compact(const std::vector<std::int32_t>& tokens,
                                      const SummaryWindow window)
    {
        SummaryResult result{};
        if (tokens.empty())
        {
            return result;
        }

        const std::size_t keepHead = std::min(window.headTokens, tokens.size());
        const std::size_t keepTail = std::min(window.tailTokens, tokens.size() - keepHead);
        const std::size_t middleTokens = tokens.size() - keepHead - keepTail;

        result.retainedTokens.reserve(keepHead + keepTail);
        result.retainedTokens.insert(result.retainedTokens.end(), tokens.begin(),
                                     tokens.begin() + static_cast<std::ptrdiff_t>(keepHead));
        if (keepTail > 0)
        {
            result.retainedTokens.insert(
                result.retainedTokens.end(),
                tokens.end() - static_cast<std::ptrdiff_t>(keepTail), tokens.end());
        }

        result.droppedTokens = middleTokens;
        result.estimatedBytes = result.retainedTokens.size() * sizeof(std::int32_t);
        return result;
    }

} // namespace us4::runtime::kv
