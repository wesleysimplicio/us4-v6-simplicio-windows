#include "runtime/core/gqa_attention.h"

#include <stdexcept>

namespace us4::core
{

    std::vector<std::size_t> BuildGqaHeadMap(const GqaConfig& config)
    {
        if (config.kvHeads == 0U || config.queryHeads == 0U)
        {
            throw std::invalid_argument("GQA requires non-zero query and KV head counts.");
        }
        if (config.queryHeads % config.kvHeads != 0U)
        {
            throw std::invalid_argument("GQA requires queryHeads to be divisible by kvHeads.");
        }

        const std::size_t groupSize = config.queryHeads / config.kvHeads;
        std::vector<std::size_t> headMap;
        headMap.reserve(config.queryHeads);
        for (std::size_t queryHead = 0; queryHead < config.queryHeads; ++queryHead)
        {
            headMap.push_back(queryHead / groupSize);
        }
        return headMap;
    }

    Tensor ExpandGroupedHeads(const Tensor& kvTensor, const GqaConfig& config)
    {
        if (kvTensor.Rank() != 3U)
        {
            throw std::invalid_argument("GQA expansion expects a rank-3 [heads, sequence, head_dim] tensor.");
        }
        if (kvTensor.Dim(0) != config.kvHeads || kvTensor.Dim(1) != config.sequenceLength ||
            kvTensor.Dim(2) != config.headDim)
        {
            throw std::invalid_argument("GQA config does not match the provided KV tensor shape.");
        }

        const auto headMap = BuildGqaHeadMap(config);
        Tensor expanded({config.queryHeads, config.sequenceLength, config.headDim}, kvTensor.DataType());
        for (std::size_t queryHead = 0; queryHead < config.queryHeads; ++queryHead)
        {
            const std::size_t kvHead = headMap[queryHead];
            for (std::size_t token = 0; token < config.sequenceLength; ++token)
            {
                for (std::size_t channel = 0; channel < config.headDim; ++channel)
                {
                    expanded.At({queryHead, token, channel}) = kvTensor.At({kvHead, token, channel});
                }
            }
        }

        return expanded;
    }

} // namespace us4::core
