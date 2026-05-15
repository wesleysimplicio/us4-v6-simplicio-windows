#include "us4/runtime/adapters/bitnet_model_loader.h"

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

        std::size_t PickContextTokens(const std::string& normalized)
        {
            if (normalized.find("128k") != std::string::npos)
            {
                return 131072;
            }
            if (normalized.find("32k") != std::string::npos)
            {
                return 32768;
            }
            return 8192;
        }
    } // namespace

    BitNetModelLoadResult LoadBitNetModelAsset(const std::string_view modelPath,
                                               const std::string_view modelId)
    {
        BitNetModelLoadResult result{};
        const ModelLoadResult base = LoadModelAsset(modelPath, modelId);
        result.ok = base.ok;
        result.message = base.message;
        result.descriptor.asset = base.descriptor;

        if (!base.ok)
        {
            return result;
        }

        const std::string normalized = Normalize(modelId);
        result.descriptor.maxContextTokens = PickContextTokens(normalized);
        result.descriptor.hiddenSize = normalized.find("3b") != std::string::npos ? 3072U : 2048U;
        result.descriptor.intermediateSize =
            normalized.find("3b") != std::string::npos ? 8192U : 5632U;
        result.descriptor.blockCount = normalized.find("3b") != std::string::npos ? 28U : 24U;
        result.descriptor.attentionHeads = normalized.find("3b") != std::string::npos ? 24U : 16U;
        result.descriptor.kvHeads = normalized.find("gqa") != std::string::npos ? 8U : 16U;
        result.descriptor.hasGateBranch = normalized.find("gate") != std::string::npos ||
                                          normalized.find("mixture") != std::string::npos;

        if (normalized.find("1.58") != std::string::npos ||
            normalized.find("bitnet") != std::string::npos)
        {
            result.descriptor.hints.weightEncoding = BitNetWeightEncoding::kOnePointFiveBit;
            result.descriptor.quantScale = 1.58F;
        }
        else
        {
            result.descriptor.hints.weightEncoding = BitNetWeightEncoding::kBinarySign;
            result.descriptor.quantScale = 1.0F;
        }

        if (normalized.find("tmac") != std::string::npos)
        {
            result.descriptor.hints.preferredKernel = BitNetKernelStyle::kTMacTiles;
        }

        if (result.descriptor.asset.synthetic)
        {
            result.descriptor.asset.modelId = std::string(modelId);
        }

        return result;
    }

} // namespace us4::runtime::adapters
