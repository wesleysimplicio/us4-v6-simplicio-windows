#pragma once

#include <cstddef>
#include <string>
#include <string_view>

namespace us4::runtime::adapters
{

    enum class ModelAssetFormat
    {
        kUnknown,
        kSynthetic,
        kGguf,
        kSafetensors,
    };

    struct ModelAssetDescriptor
    {
        std::string requestedPath;
        std::string resolvedPath;
        std::string modelId;
        ModelAssetFormat format = ModelAssetFormat::kUnknown;
        std::size_t fileBytes = 0;
        bool synthetic = false;
    };

    struct ModelLoadResult
    {
        bool ok = false;
        ModelAssetDescriptor descriptor;
        std::string message;
    };

    [[nodiscard]] ModelLoadResult LoadModelAsset(std::string_view modelPath,
                                                 std::string_view modelId);
    [[nodiscard]] std::string ToString(ModelAssetFormat format);

} // namespace us4::runtime::adapters
