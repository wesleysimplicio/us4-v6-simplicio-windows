#pragma once

#include "us4/profiles/runtime_profile.h"
#include "us4/runtime/backends/runtime_types.h"

#include <optional>
#include <string>
#include <vector>

namespace us4::profiles
{

    class ProfileCatalog
    {
      public:
        [[nodiscard]] static std::vector<RuntimeProfile> Defaults();
        [[nodiscard]] static std::optional<RuntimeProfile> FindById(const std::string& id);
        [[nodiscard]] static std::string
        RecommendId(const us4::runtime::backends::HardwareCapabilities& capabilities);
    };

} // namespace us4::profiles
