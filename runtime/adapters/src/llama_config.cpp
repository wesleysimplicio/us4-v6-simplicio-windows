#include "us4/runtime/adapters/llama_config.h"

namespace us4::runtime::adapters
{

    std::string ToString(const LlamaRopeScaling scaling)
    {
        switch (scaling)
        {
            case LlamaRopeScaling::kNone:
                return "none";
            case LlamaRopeScaling::kLinear:
                return "linear";
            case LlamaRopeScaling::kDynamic:
                return "dynamic";
            case LlamaRopeScaling::kYarn:
                return "yarn";
        }
        return "unknown";
    }

    bool Validate(const LlamaConfig& config)
    {
        if (config.numAttentionHeads == 0 || config.numKeyValueHeads == 0)
        {
            return false;
        }
        if (config.numAttentionHeads % config.numKeyValueHeads != 0)
        {
            return false;
        }
        if (config.hiddenSize == 0 || config.numHiddenLayers == 0 || config.vocabSize == 0)
        {
            return false;
        }
        return true;
    }

} // namespace us4::runtime::adapters
