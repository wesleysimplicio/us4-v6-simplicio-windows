#include "runtime/core/model_registry.h"

#include <algorithm>

namespace us4::core
{
    namespace
    {
        bool ContainsToken(std::string_view haystack, std::string_view needle)
        {
            return haystack.find(needle) != std::string_view::npos;
        }

        ModelFamily DetectFamily(std::string_view modelId)
        {
            if (ContainsToken(modelId, "qwen"))
            {
                return ModelFamily::kQwen;
            }
            if (ContainsToken(modelId, "gemma"))
            {
                return ModelFamily::kGemma;
            }
            if (ContainsToken(modelId, "llama"))
            {
                return ModelFamily::kLlama;
            }
            if (ContainsToken(modelId, "deepseek"))
            {
                return ModelFamily::kDeepSeek;
            }
            if (ContainsToken(modelId, "kimi"))
            {
                return ModelFamily::kKimi;
            }
            if (ContainsToken(modelId, "minimax"))
            {
                return ModelFamily::kMiniMax;
            }
            if (ContainsToken(modelId, "glm"))
            {
                return ModelFamily::kGlm;
            }
            if (ContainsToken(modelId, "bitnet"))
            {
                return ModelFamily::kBitNet;
            }
            return ModelFamily::kUnknown;
        }

    } // namespace

    std::vector<ModelDescriptor> ModelRegistry::Defaults()
    {
        return {
            {"qwen-0.5b", ModelFamily::kQwen, "dense-qwen", "balanced", false, true, false},
            {"gemma-3-4b", ModelFamily::kGemma, "dense-gemma", "balanced", false, true, false},
            {"llama-3.2-3b", ModelFamily::kLlama, "dense-llama", "balanced", false, true, false},
            {"deepseek-r1-distill", ModelFamily::kDeepSeek, "moe-deepseek", "degraded", true, false,
             false},
            {"kimi-k2", ModelFamily::kKimi, "moe-kimi", "degraded", true, false, false},
            {"minimax-m1", ModelFamily::kMiniMax, "moe-minimax", "micro", true, false, false},
            {"glm-4.5", ModelFamily::kGlm, "moe-glm", "micro", true, false, true},
            {"bitnet-b1.58", ModelFamily::kBitNet, "ternary-bitnet", "nano", false, false, false},
        };
    }

    std::optional<ModelDescriptor> ModelRegistry::Resolve(std::string_view modelId)
    {
        const auto defaults = Defaults();
        const auto exact = std::find_if(defaults.begin(), defaults.end(),
                                        [modelId](const ModelDescriptor& descriptor)
                                        { return descriptor.id == modelId; });
        if (exact != defaults.end())
        {
            return *exact;
        }

        const ModelFamily family = DetectFamily(modelId);
        if (family == ModelFamily::kUnknown)
        {
            return std::nullopt;
        }

        const auto familyMatch = std::find_if(defaults.begin(), defaults.end(),
                                              [family](const ModelDescriptor& descriptor)
                                              { return descriptor.family == family; });
        if (familyMatch != defaults.end())
        {
            ModelDescriptor descriptor = *familyMatch;
            descriptor.id = std::string(modelId);
            return descriptor;
        }

        return std::nullopt;
    }

    std::string ToString(ModelFamily family)
    {
        switch (family)
        {
        case ModelFamily::kQwen:
            return "qwen";
        case ModelFamily::kGemma:
            return "gemma";
        case ModelFamily::kLlama:
            return "llama";
        case ModelFamily::kDeepSeek:
            return "deepseek";
        case ModelFamily::kKimi:
            return "kimi";
        case ModelFamily::kMiniMax:
            return "minimax";
        case ModelFamily::kGlm:
            return "glm";
        case ModelFamily::kBitNet:
            return "bitnet";
        case ModelFamily::kUnknown:
            break;
        }

        return "unknown";
    }

} // namespace us4::core
