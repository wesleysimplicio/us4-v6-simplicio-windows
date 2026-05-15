#pragma once

#include "us4/runtime/adapters/ternary_adapter_contracts.h"

#include <string_view>

namespace us4::runtime::adapters
{

    struct TernaryModelLoadResult
    {
        bool ok = false;
        TernaryModelDescriptor descriptor;
        std::string message;
    };

    [[nodiscard]] TernaryModelLoadResult LoadTernaryModelAsset(std::string_view modelPath,
                                                               std::string_view modelId);

} // namespace us4::runtime::adapters
