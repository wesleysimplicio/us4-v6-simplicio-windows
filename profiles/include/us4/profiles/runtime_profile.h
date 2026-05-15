#pragma once

#include "us4/runtime/backends/runtime_types.h"

#include <cstddef>
#include <string>

namespace us4::profiles
{

    struct RuntimeProfile
    {
        std::string id;
        std::string displayName;
        us4::runtime::backends::RuntimeMode mode = us4::runtime::backends::RuntimeMode::kBalanced;
        std::string preferredBackend = "auto";
        std::size_t targetContextTokens = 4096;
        std::size_t targetBatchSize = 1;
        bool enableKvTiering = true;
        bool enableSpeculative = false;
        bool enableMoE = false;
        bool enableContinuousBatching = false;
    };

} // namespace us4::profiles
