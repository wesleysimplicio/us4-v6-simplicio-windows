#pragma once

#include "us4/runtime/adapters/bitnet_adapter_contracts.h"

#include <string_view>

namespace us4::runtime::adapters
{

    struct BitNetModelLoadResult
    {
        bool ok = false;
        BitNetModelDescriptor descriptor;
        std::string message;
    };

    [[nodiscard]] BitNetModelLoadResult LoadBitNetModelAsset(std::string_view modelPath,
                                                             std::string_view modelId);

} // namespace us4::runtime::adapters
