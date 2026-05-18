#pragma once

#include <cstddef>
#include <string>

namespace us4::runtime::backends::cpu_avx
{

    struct AmxCapability
    {
        bool tileLoad = false;
        bool tileStore = false;
        bool dpbf16ps = false;
        bool dpbssd = false;
    };

    enum class AmxPreferredIsa
    {
        kScalar,
        kAvx2,
        kAvx512,
        kAmx,
    };

    [[nodiscard]] std::string ToString(AmxPreferredIsa isa);

    [[nodiscard]] AmxCapability DetectAmxCapability();

    struct AmxDispatchPlan
    {
        AmxPreferredIsa selectedIsa = AmxPreferredIsa::kScalar;
        std::size_t tileBytes = 0;
        std::string rationale;
    };

    [[nodiscard]] AmxDispatchPlan PlanAmxOrAvx512Fallback();

} // namespace us4::runtime::backends::cpu_avx
