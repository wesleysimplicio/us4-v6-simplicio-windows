#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace us4::core
{

    enum class ModelFamily
    {
        kUnknown,
        kQwen,
        kGemma,
        kLlama,
        kDeepSeek,
        kKimi,
        kMiniMax,
        kGlm,
        kBitNet,
        kTernary,
    };

    struct ModelDescriptor
    {
        std::string id;
        ModelFamily family = ModelFamily::kUnknown;
        std::string adapterId;
        std::string defaultProfileId;
        bool supportsMoE = false;
        bool supportsSpeculative = false;
        bool supportsVision = false;
    };

    class ModelRegistry
    {
      public:
        [[nodiscard]] static std::vector<ModelDescriptor> Defaults();
        [[nodiscard]] static std::optional<ModelDescriptor> Resolve(std::string_view modelId);
    };

    [[nodiscard]] std::string ToString(ModelFamily family);

} // namespace us4::core
