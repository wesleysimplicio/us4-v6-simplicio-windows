#include "us4/runtime/adapters/model_loader.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>

namespace us4::runtime::adapters
{
    namespace
    {
        namespace fs = std::filesystem;

        bool StartsWith(std::string_view value, std::string_view prefix)
        {
            return value.substr(0, prefix.size()) == prefix;
        }

        bool IsSyntheticPath(std::string_view path)
        {
            return StartsWith(path, "builtin://") || StartsWith(path, "memory://");
        }

        ModelAssetFormat DetectFormatFromExtension(const fs::path& path)
        {
            const std::string extension = path.extension().string();
            if (extension == ".gguf")
            {
                return ModelAssetFormat::kGguf;
            }
            if (extension == ".safetensors")
            {
                return ModelAssetFormat::kSafetensors;
            }
            return ModelAssetFormat::kUnknown;
        }

        bool HasGgufMagic(const fs::path& path)
        {
            std::ifstream stream(path, std::ios::binary);
            if (!stream)
            {
                return false;
            }

            std::array<char, 4> magic{};
            stream.read(magic.data(), static_cast<std::streamsize>(magic.size()));
            return stream.gcount() == static_cast<std::streamsize>(magic.size()) &&
                   magic[0] == 'G' && magic[1] == 'G' && magic[2] == 'U' && magic[3] == 'F';
        }

        bool LooksLikeSafeTensors(const fs::path& path)
        {
            std::ifstream stream(path, std::ios::binary);
            if (!stream)
            {
                return false;
            }

            std::array<unsigned char, 8> headerBytes{};
            stream.read(reinterpret_cast<char*>(headerBytes.data()),
                        static_cast<std::streamsize>(headerBytes.size()));
            if (stream.gcount() != static_cast<std::streamsize>(headerBytes.size()))
            {
                return false;
            }

            std::uint64_t headerSize = 0;
            for (std::size_t index = 0; index < headerBytes.size(); ++index)
            {
                headerSize |= static_cast<std::uint64_t>(headerBytes[index]) << (index * 8U);
            }

            const auto fileSize = fs::file_size(path);
            return headerSize > 1U && headerSize < fileSize;
        }

        std::optional<fs::path> ResolveFromDirectory(const fs::path& directory)
        {
            const std::array preferredNames = {
                fs::path("model.gguf"),
                fs::path("model.safetensors"),
            };

            for (const auto& candidateName : preferredNames)
            {
                const fs::path candidate = directory / candidateName;
                if (fs::exists(candidate) && fs::is_regular_file(candidate))
                {
                    return candidate;
                }
            }

            for (const auto& entry : fs::directory_iterator(directory))
            {
                if (!entry.is_regular_file())
                {
                    continue;
                }

                const ModelAssetFormat format = DetectFormatFromExtension(entry.path());
                if (format == ModelAssetFormat::kUnknown)
                {
                    continue;
                }
                return entry.path();
            }

            return std::nullopt;
        }
    } // namespace

    ModelLoadResult LoadModelAsset(std::string_view modelPath, std::string_view modelId)
    {
        ModelLoadResult result{};
        result.descriptor.requestedPath = std::string(modelPath);
        result.descriptor.modelId = std::string(modelId);

        if (modelPath.empty())
        {
            result.message = "Model path is empty.";
            return result;
        }

        if (IsSyntheticPath(modelPath))
        {
            result.ok = true;
            result.descriptor.synthetic = true;
            result.descriptor.resolvedPath = std::string(modelPath);
            result.descriptor.format = ModelAssetFormat::kSynthetic;
            return result;
        }

        fs::path resolvedPath = fs::path(std::string(modelPath));
        if (!fs::exists(resolvedPath))
        {
            result.message = "Model path does not exist.";
            return result;
        }

        if (fs::is_directory(resolvedPath))
        {
            const auto candidate = ResolveFromDirectory(resolvedPath);
            if (!candidate.has_value())
            {
                result.message = "Model directory does not contain a .gguf or .safetensors file.";
                return result;
            }
            resolvedPath = *candidate;
        }

        result.descriptor.resolvedPath = resolvedPath.string();
        result.descriptor.fileBytes = static_cast<std::size_t>(fs::file_size(resolvedPath));
        result.descriptor.format = DetectFormatFromExtension(resolvedPath);

        switch (result.descriptor.format)
        {
        case ModelAssetFormat::kGguf:
            if (!HasGgufMagic(resolvedPath))
            {
                result.message = "GGUF file does not expose the expected GGUF magic header.";
                return result;
            }
            break;
        case ModelAssetFormat::kSafetensors:
            if (!LooksLikeSafeTensors(resolvedPath))
            {
                result.message =
                    "Safetensors file does not expose a valid minimal header envelope.";
                return result;
            }
            break;
        case ModelAssetFormat::kUnknown:
            result.message = "Model path must point to a .gguf or .safetensors asset.";
            return result;
        case ModelAssetFormat::kSynthetic:
            break;
        }

        result.ok = true;
        return result;
    }

    std::string ToString(const ModelAssetFormat format)
    {
        switch (format)
        {
        case ModelAssetFormat::kSynthetic:
            return "synthetic";
        case ModelAssetFormat::kGguf:
            return "gguf";
        case ModelAssetFormat::kSafetensors:
            return "safetensors";
        case ModelAssetFormat::kUnknown:
            break;
        }
        return "unknown";
    }

} // namespace us4::runtime::adapters
