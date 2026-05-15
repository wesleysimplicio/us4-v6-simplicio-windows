#pragma once

#include "us4/runtime/adapters/i_us4_windows_adapter.h"

#include <memory>
#include <string_view>

namespace us4::runtime::adapters
{

    [[nodiscard]] std::unique_ptr<IUS4WindowsAdapter>
    CreateAdapterForModelId(std::string_view modelId);

} // namespace us4::runtime::adapters
