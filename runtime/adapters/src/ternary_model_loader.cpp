#include "us4/runtime/adapters/ternary_model_loader.h"

#include "us4/runtime/adapters/model_loader.h"

#include <algorithm>
#include <cctype>
#include <string>

namespace us4::runtime::adapters
{
    namespace
    {
        std::string Normalize(std::string_view value)
        {
            std::string lowered;
            lowered.reserve(value.size());
            for (const char character : value)
            {
                lowered.push_back(
                    static_cast<char>(std::tolower(static_cast<unsigned char>(character))));
            }
            return lowered;
        }
    } // namespace

    TernaryModelLoadResult LoadTernaryModelAsset(const std::string_view modelPath,
                                                 const std::string_view modelId)
    {
        TernaryModelLoadResult result{};
        const ModelLoadResult base = LoadModelAsset(modelPath, modelId);
        result.ok = base.ok;
        result.message = base.message;
        result.descriptor.asset = base.descriptor;

        if (!base.ok)
        {
            return result;
        }

        const std::string normalized = Normalize(modelId);
        result.descriptor.hiddenSize = normalized.find("7b") != std::string::npos ? 4096U : 3072U;
        result.descriptor.intermediateSize =
            normalized.find("7b") != std::string::npos ? 14336U : 8192U;
        result.descriptor.attentionHeads = normalized.find("7b") != std::string::npos ? 32U : 24U;
        result.descriptor.kvHeads = normalized.find("gqa") != std::string::npos ? 8U : 24U;
        result.descriptor.blockCount = normalized.find("7b") != std::string::npos ? 32U : 28U;
        result.descriptor.maxContextTokens =
            normalized.find("128k") != std::string::npos ? 131072U : 16384U;
        result.descriptor.usesGroupedExperts = normalized.find("mixtral") != std::string::npos ||
                                               normalized.find("moe") != std::string::npos;

        if (normalized.find("int4") != std::string::npos)
        {
            result.descriptor.hints.activationMode = TernaryActivationMode::kInt4Probe;
            result.descriptor.quantScale = 0.5F;
        }
        else if (normalized.find("int8") != std::string::npos)
        {
            result.descriptor.hints.activationMode = TernaryActivationMode::kInt8Symmetric;
            result.descriptor.quantScale = 0.75F;
        }

        if (normalized.find("tmac") != std::string::npos)
        {
            result.descriptor.hints.preferredKernel = TernaryKernelStyle::kTMacTiles;
        }

        return result;
    }

} // namespace us4::runtime::adapters
