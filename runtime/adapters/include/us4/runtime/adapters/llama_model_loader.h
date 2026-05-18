#pragma once

#include "us4/runtime/adapters/llama_config.h"
#include "us4/runtime/adapters/model_loader.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace us4::runtime::adapters
{

    struct LlamaTensorShape
    {
        std::string name;
        std::vector<std::size_t> dimensions;
    };

    struct LlamaTokenizerDescriptor
    {
        std::string tokenizerPath;
        std::int32_t unknownTokenId = -1;
        std::unordered_map<std::string, std::int32_t> tokenToId;
        std::unordered_map<std::int32_t, std::string> idToToken;
        std::vector<std::pair<std::string, std::int32_t>> greedyVocab;
    };

    struct LlamaModelDescriptor
    {
        ModelAssetDescriptor asset;
        LlamaConfig config;
        std::string configPath;
        std::string tokenizerPath;
        std::string quantization;
        std::vector<LlamaTensorShape> tensorShapes;
        LlamaTokenizerDescriptor tokenizer;
    };

    struct LlamaModelLoadResult
    {
        bool ok = false;
        LlamaModelDescriptor descriptor;
        std::string message;
    };

    [[nodiscard]] LlamaModelLoadResult LoadLlamaModelAsset(std::string_view modelPath,
                                                           std::string_view modelId);
    [[nodiscard]] std::vector<std::int32_t>
    TokenizeLlamaPrompt(const LlamaTokenizerDescriptor& tokenizer, std::string_view prompt);
    [[nodiscard]] std::string DetokenizeLlamaTokens(const LlamaTokenizerDescriptor& tokenizer,
                                                    const std::vector<std::int32_t>& tokens);

} // namespace us4::runtime::adapters
