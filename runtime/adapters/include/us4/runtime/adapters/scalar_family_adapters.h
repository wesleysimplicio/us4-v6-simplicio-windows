#pragma once

#include "us4/runtime/adapters/i_us4_windows_adapter.h"

#include <memory>

namespace us4::runtime::adapters
{

    std::unique_ptr<IUS4WindowsAdapter> CreateQwenScalarAdapter();
    std::unique_ptr<IUS4WindowsAdapter> CreateGemmaScalarAdapter();
    std::unique_ptr<IUS4WindowsAdapter> CreateLlamaScalarAdapter();
    std::unique_ptr<IUS4WindowsAdapter> CreateBitNetScalarAdapter();
    std::unique_ptr<IUS4WindowsAdapter> CreateTernaryScalarAdapter();
    std::unique_ptr<IUS4WindowsAdapter> CreateDeepSeekMoEScalarAdapter();
    std::unique_ptr<IUS4WindowsAdapter> CreateKimiMoEScalarAdapter();

} // namespace us4::runtime::adapters
