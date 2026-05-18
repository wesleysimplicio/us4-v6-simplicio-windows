#include "us4/runtime/adapters/llama_model_loader.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <map>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <variant>

namespace us4::runtime::adapters
{
    namespace
    {
        namespace fs = std::filesystem;

        class JsonValue
        {
          public:
            using Object = std::map<std::string, JsonValue>;
            using Array = std::vector<JsonValue>;

            JsonValue() : value_(nullptr) {}
            explicit JsonValue(std::nullptr_t) : value_(nullptr) {}
            explicit JsonValue(bool value) : value_(value) {}
            explicit JsonValue(double value) : value_(value) {}
            explicit JsonValue(std::string value) : value_(std::move(value)) {}
            explicit JsonValue(Object value) : value_(std::move(value)) {}
            explicit JsonValue(Array value) : value_(std::move(value)) {}

            [[nodiscard]] bool IsNull() const
            {
                return std::holds_alternative<std::nullptr_t>(value_);
            }
            [[nodiscard]] bool IsBool() const
            {
                return std::holds_alternative<bool>(value_);
            }
            [[nodiscard]] bool IsNumber() const
            {
                return std::holds_alternative<double>(value_);
            }
            [[nodiscard]] bool IsString() const
            {
                return std::holds_alternative<std::string>(value_);
            }
            [[nodiscard]] bool IsObject() const
            {
                return std::holds_alternative<Object>(value_);
            }
            [[nodiscard]] bool IsArray() const
            {
                return std::holds_alternative<Array>(value_);
            }

            [[nodiscard]] const bool& AsBool() const
            {
                return std::get<bool>(value_);
            }
            [[nodiscard]] const double& AsNumber() const
            {
                return std::get<double>(value_);
            }
            [[nodiscard]] const std::string& AsString() const
            {
                return std::get<std::string>(value_);
            }
            [[nodiscard]] const Object& AsObject() const
            {
                return std::get<Object>(value_);
            }
            [[nodiscard]] const Array& AsArray() const
            {
                return std::get<Array>(value_);
            }

          private:
            std::variant<std::nullptr_t, bool, double, std::string, Object, Array> value_;
        };

        class JsonParser
        {
          public:
            explicit JsonParser(std::string_view source) : source_(source) {}

            [[nodiscard]] JsonValue Parse()
            {
                const JsonValue value = ParseValue();
                SkipWhitespace();
                if (position_ != source_.size())
                {
                    throw std::runtime_error("Trailing characters after JSON document.");
                }
                return value;
            }

          private:
            [[nodiscard]] JsonValue ParseValue()
            {
                SkipWhitespace();
                if (position_ >= source_.size())
                {
                    throw std::runtime_error("Unexpected end of JSON.");
                }

                const char token = source_[position_];
                switch (token)
                {
                case '{':
                    return ParseObject();
                case '[':
                    return ParseArray();
                case '"':
                    return JsonValue(ParseString());
                case 't':
                    ConsumeLiteral("true");
                    return JsonValue(true);
                case 'f':
                    ConsumeLiteral("false");
                    return JsonValue(false);
                case 'n':
                    ConsumeLiteral("null");
                    return JsonValue(nullptr);
                default:
                    if (token == '-' || std::isdigit(static_cast<unsigned char>(token)) != 0)
                    {
                        return JsonValue(ParseNumber());
                    }
                    throw std::runtime_error("Unsupported JSON token.");
                }
            }

            [[nodiscard]] JsonValue ParseObject()
            {
                Expect('{');
                JsonValue::Object object;
                SkipWhitespace();
                if (TryConsume('}'))
                {
                    return JsonValue(std::move(object));
                }

                while (true)
                {
                    SkipWhitespace();
                    const std::string key = ParseString();
                    SkipWhitespace();
                    Expect(':');
                    object.emplace(key, ParseValue());
                    SkipWhitespace();
                    if (TryConsume('}'))
                    {
                        return JsonValue(std::move(object));
                    }
                    Expect(',');
                }
            }

            [[nodiscard]] JsonValue ParseArray()
            {
                Expect('[');
                JsonValue::Array array;
                SkipWhitespace();
                if (TryConsume(']'))
                {
                    return JsonValue(std::move(array));
                }

                while (true)
                {
                    array.push_back(ParseValue());
                    SkipWhitespace();
                    if (TryConsume(']'))
                    {
                        return JsonValue(std::move(array));
                    }
                    Expect(',');
                }
            }

            [[nodiscard]] std::string ParseString()
            {
                Expect('"');
                std::string output;
                while (position_ < source_.size())
                {
                    const char character = source_[position_++];
                    if (character == '"')
                    {
                        return output;
                    }
                    if (character == '\\')
                    {
                        if (position_ >= source_.size())
                        {
                            throw std::runtime_error("Unexpected end of JSON escape.");
                        }
                        const char escaped = source_[position_++];
                        switch (escaped)
                        {
                        case '"':
                        case '\\':
                        case '/':
                            output.push_back(escaped);
                            break;
                        case 'b':
                            output.push_back('\b');
                            break;
                        case 'f':
                            output.push_back('\f');
                            break;
                        case 'n':
                            output.push_back('\n');
                            break;
                        case 'r':
                            output.push_back('\r');
                            break;
                        case 't':
                            output.push_back('\t');
                            break;
                        default:
                            throw std::runtime_error("Unsupported JSON escape sequence.");
                        }
                        continue;
                    }
                    output.push_back(character);
                }

                throw std::runtime_error("Unterminated JSON string.");
            }

            [[nodiscard]] double ParseNumber()
            {
                const std::size_t start = position_;
                if (source_[position_] == '-')
                {
                    ++position_;
                }
                while (position_ < source_.size() &&
                       std::isdigit(static_cast<unsigned char>(source_[position_])) != 0)
                {
                    ++position_;
                }
                if (position_ < source_.size() && source_[position_] == '.')
                {
                    ++position_;
                    while (position_ < source_.size() &&
                           std::isdigit(static_cast<unsigned char>(source_[position_])) != 0)
                    {
                        ++position_;
                    }
                }

                return std::stod(std::string(source_.substr(start, position_ - start)));
            }

            void ConsumeLiteral(std::string_view literal)
            {
                if (source_.substr(position_, literal.size()) != literal)
                {
                    throw std::runtime_error("Invalid JSON literal.");
                }
                position_ += literal.size();
            }

            void SkipWhitespace()
            {
                while (position_ < source_.size() &&
                       std::isspace(static_cast<unsigned char>(source_[position_])) != 0)
                {
                    ++position_;
                }
            }

            void Expect(const char expected)
            {
                SkipWhitespace();
                if (position_ >= source_.size() || source_[position_] != expected)
                {
                    throw std::runtime_error("Unexpected JSON token.");
                }
                ++position_;
            }

            [[nodiscard]] bool TryConsume(const char expected)
            {
                SkipWhitespace();
                if (position_ < source_.size() && source_[position_] == expected)
                {
                    ++position_;
                    return true;
                }
                return false;
            }

            std::string_view source_;
            std::size_t position_ = 0;
        };

        struct GgufMetadataValue
        {
            enum class Type
            {
                kString,
                kUnsigned,
                kFloat,
                kBool,
            };

            Type type = Type::kUnsigned;
            std::string stringValue;
            std::uint64_t unsignedValue = 0;
            double floatValue = 0.0;
            bool boolValue = false;
        };

        struct GgufTensorInfo
        {
            std::string name;
            std::vector<std::size_t> dimensions;
        };

        std::string ReadTextFile(const fs::path& path)
        {
            std::ifstream stream(path, std::ios::binary);
            if (!stream)
            {
                throw std::runtime_error("Could not open text file.");
            }
            std::ostringstream buffer;
            buffer << stream.rdbuf();
            return buffer.str();
        }

        [[nodiscard]] const JsonValue* TryGetObjectField(const JsonValue::Object& object,
                                                         std::string_view key)
        {
            const auto it = object.find(std::string(key));
            return it == object.end() ? nullptr : &it->second;
        }

        [[nodiscard]] std::optional<std::size_t> TryReadSize(const JsonValue::Object& object,
                                                             std::string_view key)
        {
            const JsonValue* value = TryGetObjectField(object, key);
            if (value == nullptr || !value->IsNumber())
            {
                return std::nullopt;
            }

            const double number = value->AsNumber();
            if (number < 0.0 ||
                number > static_cast<double>(std::numeric_limits<std::size_t>::max()))
            {
                return std::nullopt;
            }
            return static_cast<std::size_t>(number);
        }

        [[nodiscard]] std::optional<float> TryReadFloat(const JsonValue::Object& object,
                                                        std::string_view key)
        {
            const JsonValue* value = TryGetObjectField(object, key);
            if (value == nullptr || !value->IsNumber())
            {
                return std::nullopt;
            }
            return static_cast<float>(value->AsNumber());
        }

        [[nodiscard]] std::optional<std::string> TryReadString(const JsonValue::Object& object,
                                                               std::string_view key)
        {
            const JsonValue* value = TryGetObjectField(object, key);
            if (value == nullptr || !value->IsString())
            {
                return std::nullopt;
            }
            return value->AsString();
        }

        [[nodiscard]] std::string Normalize(std::string_view value)
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

        [[nodiscard]] std::string InferQuantization(const fs::path& assetPath)
        {
            const std::string normalized = Normalize(assetPath.filename().string());
            if (normalized.find("q4_k_m") != std::string::npos)
            {
                return "q4_k_m";
            }
            if (normalized.find("q8_0") != std::string::npos)
            {
                return "q8_0";
            }
            if (normalized.find("q4") != std::string::npos)
            {
                return "q4";
            }
            if (normalized.find("q8") != std::string::npos)
            {
                return "q8";
            }
            return "unknown";
        }

        [[nodiscard]] LlamaConfig InferConfigFromModelId(std::string_view modelId)
        {
            const std::string normalized = Normalize(modelId);
            LlamaConfig config{};
            if (normalized.find("70b") != std::string::npos)
            {
                config.hiddenSize = 8192U;
                config.intermediateSize = 28672U;
                config.numAttentionHeads = 64U;
                config.numKeyValueHeads = 8U;
                config.numHiddenLayers = 80U;
                config.maxPositionEmbeddings = 8192U;
            }
            else if (normalized.find("8b") != std::string::npos)
            {
                config.hiddenSize = 4096U;
                config.intermediateSize = 14336U;
                config.numAttentionHeads = 32U;
                config.numKeyValueHeads = 8U;
                config.numHiddenLayers = 32U;
                config.maxPositionEmbeddings = 8192U;
            }
            else if (normalized.find("3b") != std::string::npos)
            {
                config.hiddenSize = 3072U;
                config.intermediateSize = 8192U;
                config.numAttentionHeads = 24U;
                config.numKeyValueHeads = 8U;
                config.numHiddenLayers = 28U;
                config.maxPositionEmbeddings = 8192U;
            }
            else if (normalized.find("1b") != std::string::npos ||
                     normalized.find("mini") != std::string::npos)
            {
                config.hiddenSize = 2048U;
                config.intermediateSize = 5632U;
                config.numAttentionHeads = 16U;
                config.numKeyValueHeads = 8U;
                config.numHiddenLayers = 16U;
                config.maxPositionEmbeddings = 8192U;
            }
            config.architecture = "llama";
            return config;
        }

        [[nodiscard]] std::string ResolveModelRoot(const fs::path& modelPath,
                                                   const ModelLoadResult& base)
        {
            const fs::path requested(modelPath);
            if (fs::exists(requested) && fs::is_directory(requested))
            {
                return requested.string();
            }

            const fs::path resolved(base.descriptor.resolvedPath);
            return resolved.parent_path().string();
        }

        void PopulateTokenizer(const JsonValue& document, LlamaTokenizerDescriptor& tokenizer)
        {
            if (!document.IsObject())
            {
                throw std::runtime_error("tokenizer.json root must be an object.");
            }

            const auto& root = document.AsObject();
            const JsonValue* model = TryGetObjectField(root, "model");
            if (model == nullptr || !model->IsObject())
            {
                throw std::runtime_error("tokenizer.json is missing model.vocab.");
            }

            const JsonValue* vocabValue = TryGetObjectField(model->AsObject(), "vocab");
            if (vocabValue == nullptr || !vocabValue->IsObject())
            {
                throw std::runtime_error("tokenizer.json is missing model.vocab.");
            }

            for (const auto& [piece, token] : vocabValue->AsObject())
            {
                if (!token.IsNumber())
                {
                    continue;
                }
                const auto id = static_cast<std::int32_t>(token.AsNumber());
                tokenizer.tokenToId.emplace(piece, id);
                tokenizer.idToToken.emplace(id, piece);
                tokenizer.greedyVocab.emplace_back(piece, id);
            }

            const JsonValue* addedTokens = TryGetObjectField(root, "added_tokens");
            if (addedTokens != nullptr && addedTokens->IsArray())
            {
                for (const JsonValue& entry : addedTokens->AsArray())
                {
                    if (!entry.IsObject())
                    {
                        continue;
                    }
                    const auto& object = entry.AsObject();
                    const auto content = TryReadString(object, "content");
                    const auto id = TryReadSize(object, "id");
                    if (!content.has_value() || !id.has_value())
                    {
                        continue;
                    }

                    const auto tokenId = static_cast<std::int32_t>(*id);
                    tokenizer.tokenToId[*content] = tokenId;
                    tokenizer.idToToken[tokenId] = *content;
                    tokenizer.greedyVocab.emplace_back(*content, tokenId);
                }
            }

            const auto unkToken = tokenizer.tokenToId.find("<unk>");
            if (unkToken != tokenizer.tokenToId.end())
            {
                tokenizer.unknownTokenId = unkToken->second;
            }

            std::sort(tokenizer.greedyVocab.begin(), tokenizer.greedyVocab.end(),
                      [](const auto& left, const auto& right)
                      {
                          if (left.first.size() == right.first.size())
                          {
                              return left.second < right.second;
                          }
                          return left.first.size() > right.first.size();
                      });

            if (tokenizer.greedyVocab.empty())
            {
                throw std::runtime_error("tokenizer.json vocab is empty.");
            }
        }

        LlamaTokenizerDescriptor LoadTokenizer(const fs::path& tokenizerPath)
        {
            LlamaTokenizerDescriptor tokenizer{};
            tokenizer.tokenizerPath = tokenizerPath.string();
            const JsonValue document = JsonParser(ReadTextFile(tokenizerPath)).Parse();
            PopulateTokenizer(document, tokenizer);
            return tokenizer;
        }

        void MergeConfigJson(const fs::path& configPath, LlamaConfig& config)
        {
            const JsonValue document = JsonParser(ReadTextFile(configPath)).Parse();
            if (!document.IsObject())
            {
                throw std::runtime_error("config.json root must be an object.");
            }

            const auto& object = document.AsObject();
            if (const auto hiddenSize = TryReadSize(object, "hidden_size"); hiddenSize.has_value())
            {
                config.hiddenSize = *hiddenSize;
            }
            if (const auto intermediateSize = TryReadSize(object, "intermediate_size");
                intermediateSize.has_value())
            {
                config.intermediateSize = *intermediateSize;
            }
            if (const auto attentionHeads = TryReadSize(object, "num_attention_heads");
                attentionHeads.has_value())
            {
                config.numAttentionHeads = *attentionHeads;
            }
            if (const auto kvHeads = TryReadSize(object, "num_key_value_heads");
                kvHeads.has_value())
            {
                config.numKeyValueHeads = *kvHeads;
            }
            if (const auto layers = TryReadSize(object, "num_hidden_layers"); layers.has_value())
            {
                config.numHiddenLayers = *layers;
            }
            if (const auto vocabSize = TryReadSize(object, "vocab_size"); vocabSize.has_value())
            {
                config.vocabSize = *vocabSize;
            }
            if (const auto maxPosition = TryReadSize(object, "max_position_embeddings");
                maxPosition.has_value())
            {
                config.maxPositionEmbeddings = *maxPosition;
            }
            if (const auto ropeBase = TryReadFloat(object, "rope_theta"); ropeBase.has_value())
            {
                config.ropeBase = *ropeBase;
            }
            if (const auto architecture = TryReadString(object, "model_type");
                architecture.has_value())
            {
                config.architecture = *architecture;
            }

            const JsonValue* ropeScaling = TryGetObjectField(object, "rope_scaling");
            if (ropeScaling != nullptr && ropeScaling->IsObject())
            {
                const auto& ropeObject = ropeScaling->AsObject();
                if (const auto factor = TryReadFloat(ropeObject, "factor"); factor.has_value())
                {
                    config.ropeScalingFactor = *factor;
                }
                if (const auto scalingType = TryReadString(ropeObject, "type");
                    scalingType.has_value())
                {
                    const std::string normalized = Normalize(*scalingType);
                    if (normalized == "linear")
                    {
                        config.ropeScaling = LlamaRopeScaling::kLinear;
                    }
                    else if (normalized == "dynamic")
                    {
                        config.ropeScaling = LlamaRopeScaling::kDynamic;
                    }
                    else if (normalized == "yarn")
                    {
                        config.ropeScaling = LlamaRopeScaling::kYarn;
                    }
                }
            }
        }

        template <typename TValue> TValue ReadBinary(std::istream& stream)
        {
            TValue value{};
            stream.read(reinterpret_cast<char*>(&value),
                        static_cast<std::streamsize>(sizeof(TValue)));
            if (!stream)
            {
                throw std::runtime_error("Unexpected end of binary metadata.");
            }
            return value;
        }

        std::string ReadGgufString(std::istream& stream)
        {
            const auto length = ReadBinary<std::uint64_t>(stream);
            std::string value(length, '\0');
            stream.read(value.data(), static_cast<std::streamsize>(length));
            if (!stream)
            {
                throw std::runtime_error("Unexpected end of GGUF string.");
            }
            return value;
        }

        GgufMetadataValue ReadGgufMetadataValue(std::istream& stream)
        {
            const auto type = ReadBinary<std::uint32_t>(stream);
            GgufMetadataValue value{};
            switch (type)
            {
            case 4U:
                value.type = GgufMetadataValue::Type::kUnsigned;
                value.unsignedValue = ReadBinary<std::uint32_t>(stream);
                break;
            case 6U:
                value.type = GgufMetadataValue::Type::kFloat;
                value.floatValue = ReadBinary<float>(stream);
                break;
            case 7U:
                value.type = GgufMetadataValue::Type::kBool;
                value.boolValue = ReadBinary<std::uint8_t>(stream) != 0U;
                break;
            case 8U:
                value.type = GgufMetadataValue::Type::kString;
                value.stringValue = ReadGgufString(stream);
                break;
            case 10U:
                value.type = GgufMetadataValue::Type::kUnsigned;
                value.unsignedValue = ReadBinary<std::uint64_t>(stream);
                break;
            default:
                throw std::runtime_error("Unsupported GGUF metadata type.");
            }
            return value;
        }

        void ParseGgufMetadata(const fs::path& assetPath, LlamaConfig& config,
                               std::vector<LlamaTensorShape>& tensorShapes)
        {
            std::ifstream stream(assetPath, std::ios::binary);
            if (!stream)
            {
                throw std::runtime_error("Could not open GGUF asset.");
            }

            std::array<char, 4> magic{};
            stream.read(magic.data(), static_cast<std::streamsize>(magic.size()));
            if (magic != std::array<char, 4>{'G', 'G', 'U', 'F'})
            {
                throw std::runtime_error("Invalid GGUF magic.");
            }

            const auto version = ReadBinary<std::uint32_t>(stream);
            if (version < 2U)
            {
                throw std::runtime_error("Unsupported GGUF version.");
            }

            const auto tensorCount = ReadBinary<std::uint64_t>(stream);
            const auto metadataCount = ReadBinary<std::uint64_t>(stream);
            std::map<std::string, GgufMetadataValue> metadata;
            for (std::uint64_t index = 0; index < metadataCount; ++index)
            {
                const std::string key = ReadGgufString(stream);
                metadata.emplace(key, ReadGgufMetadataValue(stream));
            }

            if (const auto it = metadata.find("general.architecture");
                it != metadata.end() && it->second.type == GgufMetadataValue::Type::kString)
            {
                config.architecture = it->second.stringValue;
            }
            if (const auto it = metadata.find("llama.embedding_length");
                it != metadata.end() && it->second.type == GgufMetadataValue::Type::kUnsigned)
            {
                config.hiddenSize = static_cast<std::size_t>(it->second.unsignedValue);
            }
            if (const auto it = metadata.find("llama.feed_forward_length");
                it != metadata.end() && it->second.type == GgufMetadataValue::Type::kUnsigned)
            {
                config.intermediateSize = static_cast<std::size_t>(it->second.unsignedValue);
            }
            if (const auto it = metadata.find("llama.attention.head_count");
                it != metadata.end() && it->second.type == GgufMetadataValue::Type::kUnsigned)
            {
                config.numAttentionHeads = static_cast<std::size_t>(it->second.unsignedValue);
            }
            if (const auto it = metadata.find("llama.attention.head_count_kv");
                it != metadata.end() && it->second.type == GgufMetadataValue::Type::kUnsigned)
            {
                config.numKeyValueHeads = static_cast<std::size_t>(it->second.unsignedValue);
            }
            if (const auto it = metadata.find("llama.block_count");
                it != metadata.end() && it->second.type == GgufMetadataValue::Type::kUnsigned)
            {
                config.numHiddenLayers = static_cast<std::size_t>(it->second.unsignedValue);
            }
            if (const auto it = metadata.find("llama.context_length");
                it != metadata.end() && it->second.type == GgufMetadataValue::Type::kUnsigned)
            {
                config.maxPositionEmbeddings = static_cast<std::size_t>(it->second.unsignedValue);
            }
            if (const auto it = metadata.find("llama.rope.freq_base"); it != metadata.end())
            {
                if (it->second.type == GgufMetadataValue::Type::kFloat)
                {
                    config.ropeBase = static_cast<float>(it->second.floatValue);
                }
                else if (it->second.type == GgufMetadataValue::Type::kUnsigned)
                {
                    config.ropeBase = static_cast<float>(it->second.unsignedValue);
                }
            }

            tensorShapes.clear();
            tensorShapes.reserve(static_cast<std::size_t>(tensorCount));
            for (std::uint64_t tensorIndex = 0; tensorIndex < tensorCount; ++tensorIndex)
            {
                LlamaTensorShape tensor{};
                tensor.name = ReadGgufString(stream);
                const auto dimensions = ReadBinary<std::uint32_t>(stream);
                tensor.dimensions.reserve(dimensions);
                for (std::uint32_t dimension = 0; dimension < dimensions; ++dimension)
                {
                    tensor.dimensions.push_back(
                        static_cast<std::size_t>(ReadBinary<std::uint64_t>(stream)));
                }

                static_cast<void>(ReadBinary<std::uint32_t>(stream));
                static_cast<void>(ReadBinary<std::uint64_t>(stream));
                tensorShapes.push_back(std::move(tensor));
            }
        }

        void ParseSafeTensorsMetadata(const fs::path& assetPath,
                                      std::vector<LlamaTensorShape>& tensorShapes)
        {
            std::ifstream stream(assetPath, std::ios::binary);
            if (!stream)
            {
                throw std::runtime_error("Could not open safetensors asset.");
            }

            const std::uint64_t headerSize = ReadBinary<std::uint64_t>(stream);
            std::string header(headerSize, '\0');
            stream.read(header.data(), static_cast<std::streamsize>(headerSize));
            if (!stream)
            {
                throw std::runtime_error("Could not read safetensors header.");
            }

            const JsonValue document = JsonParser(header).Parse();
            if (!document.IsObject())
            {
                throw std::runtime_error("Safetensors header must be a JSON object.");
            }

            tensorShapes.clear();
            for (const auto& [name, value] : document.AsObject())
            {
                if (name == "__metadata__" || !value.IsObject())
                {
                    continue;
                }

                const JsonValue* shape = TryGetObjectField(value.AsObject(), "shape");
                if (shape == nullptr || !shape->IsArray())
                {
                    continue;
                }

                LlamaTensorShape tensor{};
                tensor.name = name;
                for (const JsonValue& dimension : shape->AsArray())
                {
                    if (!dimension.IsNumber())
                    {
                        throw std::runtime_error("Safetensors shape dimension is not numeric.");
                    }
                    tensor.dimensions.push_back(static_cast<std::size_t>(dimension.AsNumber()));
                }
                tensorShapes.push_back(std::move(tensor));
            }
        }

        [[nodiscard]] const LlamaTensorShape*
        FindTensor(const std::vector<LlamaTensorShape>& tensorShapes, std::string_view name)
        {
            const auto it =
                std::find_if(tensorShapes.begin(), tensorShapes.end(),
                             [name](const auto& tensor) { return tensor.name == name; });
            return it == tensorShapes.end() ? nullptr : &*it;
        }

        [[nodiscard]] bool ShapeContains(const std::vector<std::size_t>& shape,
                                         const std::size_t value)
        {
            return std::find(shape.begin(), shape.end(), value) != shape.end();
        }

        [[nodiscard]] bool ValidateTensorShape(const LlamaTensorShape& tensor,
                                               const std::size_t left, const std::size_t right)
        {
            return tensor.dimensions.size() == 2U && ShapeContains(tensor.dimensions, left) &&
                   ShapeContains(tensor.dimensions, right);
        }

        void PopulateConfigFromTensorShapes(const std::vector<LlamaTensorShape>& tensorShapes,
                                            LlamaConfig& config)
        {
            if (const auto* tokenEmbedding = FindTensor(tensorShapes, "token_embd.weight");
                tokenEmbedding != nullptr && tokenEmbedding->dimensions.size() == 2U)
            {
                config.vocabSize =
                    std::max(tokenEmbedding->dimensions[0], tokenEmbedding->dimensions[1]);
                config.hiddenSize =
                    std::min(tokenEmbedding->dimensions[0], tokenEmbedding->dimensions[1]);
            }

            std::size_t detectedBlocks = 0;
            for (const auto& tensor : tensorShapes)
            {
                const std::string prefix = "blk.";
                if (tensor.name.rfind(prefix, 0) != 0)
                {
                    continue;
                }

                const auto separator = tensor.name.find('.', prefix.size());
                if (separator == std::string::npos)
                {
                    continue;
                }

                try
                {
                    const auto index = static_cast<std::size_t>(
                        std::stoul(tensor.name.substr(prefix.size(), separator - prefix.size())));
                    detectedBlocks = std::max(detectedBlocks, index + 1U);
                }
                catch (const std::exception&)
                {
                }
            }

            if (detectedBlocks > 0U)
            {
                config.numHiddenLayers = detectedBlocks;
            }
        }

        void ValidateTensorShapes(const LlamaConfig& config,
                                  const std::vector<LlamaTensorShape>& tensorShapes)
        {
            if (const auto* tokenEmbedding = FindTensor(tensorShapes, "token_embd.weight");
                tokenEmbedding != nullptr &&
                !ValidateTensorShape(*tokenEmbedding, config.vocabSize, config.hiddenSize))
            {
                throw std::runtime_error(
                    "token_embd.weight does not match the inferred Llama shape.");
            }

            if (const auto* output = FindTensor(tensorShapes, "output.weight");
                output != nullptr &&
                !ValidateTensorShape(*output, config.vocabSize, config.hiddenSize))
            {
                throw std::runtime_error("output.weight does not match the inferred Llama shape.");
            }

            const std::size_t kvHiddenSize =
                config.numAttentionHeads == 0U
                    ? config.hiddenSize
                    : (config.hiddenSize / config.numAttentionHeads) * config.numKeyValueHeads;

            if (const auto* attnQ = FindTensor(tensorShapes, "blk.0.attn_q.weight");
                attnQ != nullptr &&
                !ValidateTensorShape(*attnQ, config.hiddenSize, config.hiddenSize))
            {
                throw std::runtime_error(
                    "blk.0.attn_q.weight does not match the inferred Llama shape.");
            }
            if (const auto* attnK = FindTensor(tensorShapes, "blk.0.attn_k.weight");
                attnK != nullptr && !ValidateTensorShape(*attnK, config.hiddenSize, kvHiddenSize))
            {
                throw std::runtime_error(
                    "blk.0.attn_k.weight does not match the inferred Llama shape.");
            }
            if (const auto* attnV = FindTensor(tensorShapes, "blk.0.attn_v.weight");
                attnV != nullptr && !ValidateTensorShape(*attnV, config.hiddenSize, kvHiddenSize))
            {
                throw std::runtime_error(
                    "blk.0.attn_v.weight does not match the inferred Llama shape.");
            }
            if (const auto* ffnUp = FindTensor(tensorShapes, "blk.0.ffn_up.weight");
                ffnUp != nullptr &&
                !ValidateTensorShape(*ffnUp, config.intermediateSize, config.hiddenSize))
            {
                throw std::runtime_error(
                    "blk.0.ffn_up.weight does not match the inferred Llama shape.");
            }
            if (const auto* ffnDown = FindTensor(tensorShapes, "blk.0.ffn_down.weight");
                ffnDown != nullptr &&
                !ValidateTensorShape(*ffnDown, config.intermediateSize, config.hiddenSize))
            {
                throw std::runtime_error(
                    "blk.0.ffn_down.weight does not match the inferred Llama shape.");
            }
        }
    } // namespace

    LlamaModelLoadResult LoadLlamaModelAsset(const std::string_view modelPath,
                                             const std::string_view modelId)
    {
        LlamaModelLoadResult result{};
        const ModelLoadResult base = LoadModelAsset(modelPath, modelId);
        result.ok = base.ok;
        result.message = base.message;
        result.descriptor.asset = base.descriptor;
        result.descriptor.config = InferConfigFromModelId(modelId);

        if (!base.ok)
        {
            return result;
        }

        try
        {
            const fs::path modelRoot(ResolveModelRoot(fs::path(std::string(modelPath)), base));
            const fs::path tokenizerPath = modelRoot / "tokenizer.json";
            if (!fs::exists(tokenizerPath))
            {
                result.ok = false;
                result.message = "Llama model directory is missing tokenizer.json.";
                return result;
            }

            result.descriptor.tokenizerPath = tokenizerPath.string();
            result.descriptor.tokenizer = LoadTokenizer(tokenizerPath);
            result.descriptor.quantization =
                InferQuantization(fs::path(base.descriptor.resolvedPath));

            const fs::path configPath = modelRoot / "config.json";
            if (fs::exists(configPath))
            {
                result.descriptor.configPath = configPath.string();
                MergeConfigJson(configPath, result.descriptor.config);
            }

            if (base.descriptor.format == ModelAssetFormat::kGguf)
            {
                ParseGgufMetadata(fs::path(base.descriptor.resolvedPath), result.descriptor.config,
                                  result.descriptor.tensorShapes);
            }
            else if (base.descriptor.format == ModelAssetFormat::kSafetensors)
            {
                ParseSafeTensorsMetadata(fs::path(base.descriptor.resolvedPath),
                                         result.descriptor.tensorShapes);
                if (result.descriptor.configPath.empty())
                {
                    PopulateConfigFromTensorShapes(result.descriptor.tensorShapes,
                                                   result.descriptor.config);
                }
            }

            ValidateTensorShapes(result.descriptor.config, result.descriptor.tensorShapes);
            if (!Validate(result.descriptor.config))
            {
                result.ok = false;
                result.message = "Resolved Llama config is invalid.";
                return result;
            }

            result.ok = true;
            result.message.clear();
            return result;
        }
        catch (const std::exception& exception)
        {
            result.ok = false;
            result.message = exception.what();
            return result;
        }
    }

    std::vector<std::int32_t> TokenizeLlamaPrompt(const LlamaTokenizerDescriptor& tokenizer,
                                                  const std::string_view prompt)
    {
        std::vector<std::int32_t> tokens;
        std::size_t index = 0;
        while (index < prompt.size())
        {
            std::size_t matchedLength = 0;
            std::int32_t matchedToken = tokenizer.unknownTokenId;

            for (const auto& [piece, tokenId] : tokenizer.greedyVocab)
            {
                if (piece.empty() || piece.size() > prompt.size() - index)
                {
                    continue;
                }
                if (prompt.substr(index, piece.size()) == piece)
                {
                    matchedLength = piece.size();
                    matchedToken = tokenId;
                    break;
                }
            }

            if (matchedLength == 0U)
            {
                if (tokenizer.unknownTokenId >= 0)
                {
                    tokens.push_back(tokenizer.unknownTokenId);
                }
                ++index;
                continue;
            }

            tokens.push_back(matchedToken);
            index += matchedLength;
        }
        return tokens;
    }

    std::string DetokenizeLlamaTokens(const LlamaTokenizerDescriptor& tokenizer,
                                      const std::vector<std::int32_t>& tokens)
    {
        std::string prompt;
        for (const auto token : tokens)
        {
            const auto it = tokenizer.idToToken.find(token);
            if (it == tokenizer.idToToken.end())
            {
                continue;
            }
            prompt += it->second;
        }
        return prompt;
    }

} // namespace us4::runtime::adapters
