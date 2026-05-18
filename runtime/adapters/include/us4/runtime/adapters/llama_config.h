#pragma once

#include <cstddef>
#include <string>

namespace us4::runtime::adapters
{

    enum class LlamaRopeScaling
    {
        kNone,
        kLinear,
        kDynamic,
        kYarn,
    };

    [[nodiscard]] std::string ToString(LlamaRopeScaling scaling);

    struct LlamaConfig
    {
        std::size_t hiddenSize = 4096;
        std::size_t intermediateSize = 11008;
        std::size_t numAttentionHeads = 32;
        std::size_t numKeyValueHeads = 8;
        std::size_t numHiddenLayers = 32;
        std::size_t vocabSize = 128000;
        std::size_t maxPositionEmbeddings = 8192;
        float ropeBase = 10000.0F;
        float ropeScalingFactor = 1.0F;
        LlamaRopeScaling ropeScaling = LlamaRopeScaling::kNone;
        bool useAlibi = false;
        std::string architecture = "llama";
    };

    [[nodiscard]] bool Validate(const LlamaConfig& config);

} // namespace us4::runtime::adapters
