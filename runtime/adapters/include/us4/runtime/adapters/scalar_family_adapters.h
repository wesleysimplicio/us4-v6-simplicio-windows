#pragma once

#include "us4/runtime/adapters/i_us4_windows_adapter.h"

#include <memory>

namespace us4::runtime::adapters
{

    std::unique_ptr<IUS4WindowsAdapter> CreateQwenScalarAdapter();
    std::unique_ptr<IUS4WindowsAdapter> CreateGemmaScalarAdapter();

} // namespace us4::runtime::adapters
