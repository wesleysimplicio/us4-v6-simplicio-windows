#include "us4/runtime/adapters/llama_model_loader.h"

#include <array>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>

namespace us4::runtime::tests
{
    namespace
    {
        namespace fs = std::filesystem;

        class ScopedFixtureDirectory
        {
          public:
            ScopedFixtureDirectory()
            {
                path_ = fs::temp_directory_path() /
                        fs::path("us4-llama-loader-" + std::to_string(counter_++));
                fs::create_directories(path_);
            }

            ~ScopedFixtureDirectory()
            {
                std::error_code ignored;
                fs::remove_all(path_, ignored);
            }

            [[nodiscard]] const fs::path& Path() const
            {
                return path_;
            }

          private:
            fs::path path_;
            inline static std::size_t counter_ = 0U;
        };

        void WriteTextFile(const fs::path& path, const std::string& content)
        {
            std::ofstream stream(path, std::ios::binary);
            ASSERT_TRUE(stream.is_open());
            stream << content;
        }

        template <typename TValue> void WriteBinaryValue(std::ofstream& stream, const TValue value)
        {
            stream.write(reinterpret_cast<const char*>(&value),
                         static_cast<std::streamsize>(sizeof(TValue)));
        }

        void WriteGgufString(std::ofstream& stream, const std::string& value)
        {
            WriteBinaryValue<std::uint64_t>(stream, static_cast<std::uint64_t>(value.size()));
            stream.write(value.data(), static_cast<std::streamsize>(value.size()));
        }

        void WriteMinimalTokenizer(const fs::path& directory)
        {
            WriteTextFile(directory / "tokenizer.json",
                          R"({
  "model": {
    "type": "BPE",
    "vocab": {
      "hello": 0,
      " ": 1,
      "llama": 2,
      "runtime": 3,
      "<unk>": 4
    },
    "merges": []
  },
  "added_tokens": [
    { "id": 128000, "content": "<|begin_of_text|>", "special": true }
  ]
})");
        }

        void WriteMinimalConfig(const fs::path& directory)
        {
            WriteTextFile(directory / "config.json",
                          R"({
  "model_type": "llama",
  "hidden_size": 3072,
  "intermediate_size": 8192,
  "num_attention_heads": 24,
  "num_key_value_heads": 8,
  "num_hidden_layers": 28,
  "vocab_size": 128000,
  "max_position_embeddings": 8192,
  "rope_theta": 500000.0
})");
        }

        void WriteMinimalSafeTensors(const fs::path& path)
        {
            const std::string header = R"({
  "token_embd.weight": {"dtype":"F16","shape":[128000,3072],"data_offsets":[0,16]},
  "output.weight": {"dtype":"F16","shape":[128000,3072],"data_offsets":[16,32]},
  "blk.0.attn_q.weight": {"dtype":"F16","shape":[3072,3072],"data_offsets":[32,48]},
  "blk.0.attn_k.weight": {"dtype":"F16","shape":[3072,1024],"data_offsets":[48,64]},
  "blk.0.attn_v.weight": {"dtype":"F16","shape":[3072,1024],"data_offsets":[64,80]},
  "blk.0.ffn_up.weight": {"dtype":"F16","shape":[8192,3072],"data_offsets":[80,96]},
  "blk.0.ffn_down.weight": {"dtype":"F16","shape":[3072,8192],"data_offsets":[96,112]}
})";

            std::ofstream stream(path, std::ios::binary);
            ASSERT_TRUE(stream.is_open());
            const std::uint64_t headerSize = static_cast<std::uint64_t>(header.size());
            WriteBinaryValue(stream, headerSize);
            stream.write(header.data(), static_cast<std::streamsize>(header.size()));
            const std::array<char, 128> payload{};
            stream.write(payload.data(), static_cast<std::streamsize>(payload.size()));
        }

        void WriteMismatchedSafeTensors(const fs::path& path)
        {
            const std::string header = R"({
  "token_embd.weight": {"dtype":"F16","shape":[128000,2048],"data_offsets":[0,16]}
})";

            std::ofstream stream(path, std::ios::binary);
            ASSERT_TRUE(stream.is_open());
            const std::uint64_t headerSize = static_cast<std::uint64_t>(header.size());
            WriteBinaryValue(stream, headerSize);
            stream.write(header.data(), static_cast<std::streamsize>(header.size()));
            const std::array<char, 32> payload{};
            stream.write(payload.data(), static_cast<std::streamsize>(payload.size()));
        }

        void WriteMinimalGguf(const fs::path& path)
        {
            std::ofstream stream(path, std::ios::binary);
            ASSERT_TRUE(stream.is_open());

            stream.write("GGUF", 4);
            WriteBinaryValue<std::uint32_t>(stream, 3U);
            WriteBinaryValue<std::uint64_t>(stream, 6ULL);
            WriteBinaryValue<std::uint64_t>(stream, 7ULL);

            auto writeUnsignedMetadata =
                [&stream](const std::string& key, const std::uint32_t value)
            {
                WriteGgufString(stream, key);
                WriteBinaryValue<std::uint32_t>(stream, 4U);
                WriteBinaryValue<std::uint32_t>(stream, value);
            };

            WriteGgufString(stream, "general.architecture");
            WriteBinaryValue<std::uint32_t>(stream, 8U);
            WriteGgufString(stream, "llama");

            writeUnsignedMetadata("llama.embedding_length", 3072U);
            writeUnsignedMetadata("llama.feed_forward_length", 8192U);
            writeUnsignedMetadata("llama.attention.head_count", 24U);
            writeUnsignedMetadata("llama.attention.head_count_kv", 8U);
            writeUnsignedMetadata("llama.block_count", 28U);
            writeUnsignedMetadata("llama.context_length", 8192U);

            auto writeTensor = [&stream](const std::string& name,
                                         const std::initializer_list<std::uint64_t> dimensions)
            {
                WriteGgufString(stream, name);
                WriteBinaryValue<std::uint32_t>(stream,
                                                static_cast<std::uint32_t>(dimensions.size()));
                for (const auto dimension : dimensions)
                {
                    WriteBinaryValue<std::uint64_t>(stream, dimension);
                }
                WriteBinaryValue<std::uint32_t>(stream, 1U);
                WriteBinaryValue<std::uint64_t>(stream, 0ULL);
            };

            writeTensor("token_embd.weight", {128000ULL, 3072ULL});
            writeTensor("output.weight", {128000ULL, 3072ULL});
            writeTensor("blk.0.attn_q.weight", {3072ULL, 3072ULL});
            writeTensor("blk.0.attn_k.weight", {3072ULL, 1024ULL});
            writeTensor("blk.0.attn_v.weight", {3072ULL, 1024ULL});
            writeTensor("blk.0.ffn_up.weight", {8192ULL, 3072ULL});
        }

        TEST(LlamaModelLoaderTest, LoadsSafeTensorsWithTokenizerAndConfig)
        {
            ScopedFixtureDirectory fixture;
            WriteMinimalTokenizer(fixture.Path());
            WriteMinimalConfig(fixture.Path());
            WriteMinimalSafeTensors(fixture.Path() / "model-q8_0.safetensors");

            const auto result = us4::runtime::adapters::LoadLlamaModelAsset(fixture.Path().string(),
                                                                            "llama-3.2-3b");

            ASSERT_TRUE(result.ok) << result.message;
            EXPECT_EQ(result.descriptor.asset.format,
                      us4::runtime::adapters::ModelAssetFormat::kSafetensors);
            EXPECT_EQ(result.descriptor.quantization, "q8_0");
            EXPECT_EQ(result.descriptor.config.hiddenSize, 3072U);
            EXPECT_EQ(result.descriptor.config.numAttentionHeads, 24U);
            EXPECT_EQ(result.descriptor.tensorShapes.size(), 7U);
            EXPECT_FALSE(result.descriptor.tokenizer.greedyVocab.empty());
        }

        TEST(LlamaModelLoaderTest, LoadsGgufMetadataAndTensorShapes)
        {
            ScopedFixtureDirectory fixture;
            WriteMinimalTokenizer(fixture.Path());
            WriteMinimalGguf(fixture.Path() / "model-q4_k_m.gguf");

            const auto result = us4::runtime::adapters::LoadLlamaModelAsset(fixture.Path().string(),
                                                                            "llama-3.2-3b");

            ASSERT_TRUE(result.ok) << result.message;
            EXPECT_EQ(result.descriptor.asset.format,
                      us4::runtime::adapters::ModelAssetFormat::kGguf);
            EXPECT_EQ(result.descriptor.quantization, "q4_k_m");
            EXPECT_EQ(result.descriptor.config.architecture, "llama");
            EXPECT_EQ(result.descriptor.config.hiddenSize, 3072U);
            EXPECT_EQ(result.descriptor.config.numKeyValueHeads, 8U);
            EXPECT_GE(result.descriptor.tensorShapes.size(), 6U);
        }

        TEST(LlamaModelLoaderTest, TokenizerRoundTripsKnownPromptPieces)
        {
            ScopedFixtureDirectory fixture;
            WriteMinimalTokenizer(fixture.Path());
            WriteMinimalConfig(fixture.Path());
            WriteMinimalSafeTensors(fixture.Path() / "model.safetensors");

            const auto result = us4::runtime::adapters::LoadLlamaModelAsset(fixture.Path().string(),
                                                                            "llama-3.2-3b");

            ASSERT_TRUE(result.ok) << result.message;
            const std::string prompt = "hello llama runtime";
            const auto tokens =
                us4::runtime::adapters::TokenizeLlamaPrompt(result.descriptor.tokenizer, prompt);

            EXPECT_EQ(
                us4::runtime::adapters::DetokenizeLlamaTokens(result.descriptor.tokenizer, tokens),
                prompt);
            EXPECT_EQ(tokens, (std::vector<std::int32_t>{0, 1, 2, 1, 3}));
        }

        TEST(LlamaModelLoaderTest, RejectsShapeMismatchAgainstResolvedConfig)
        {
            ScopedFixtureDirectory fixture;
            WriteMinimalTokenizer(fixture.Path());
            WriteMinimalConfig(fixture.Path());
            WriteMismatchedSafeTensors(fixture.Path() / "model.safetensors");

            const auto result = us4::runtime::adapters::LoadLlamaModelAsset(fixture.Path().string(),
                                                                            "llama-3.2-3b");

            EXPECT_EQ(result.descriptor.config.hiddenSize, 3072U);
            EXPECT_EQ(result.descriptor.tensorShapes.size(), 1U);
            ASSERT_FALSE(result.ok);
            EXPECT_NE(result.message.find("token_embd.weight"), std::string::npos);
        }

    } // namespace
} // namespace us4::runtime::tests
