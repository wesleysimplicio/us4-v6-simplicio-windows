#include "us4/runtime/backends/cpu_avx/amx_dispatcher.h"

#include <sstream>

namespace us4::runtime::backends::cpu_avx
{

    std::string ToString(const AmxPreferredIsa isa)
    {
        switch (isa)
        {
            case AmxPreferredIsa::kScalar:
                return "scalar";
            case AmxPreferredIsa::kAvx2:
                return "avx2";
            case AmxPreferredIsa::kAvx512:
                return "avx512";
            case AmxPreferredIsa::kAmx:
                return "amx";
        }
        return "unknown";
    }

    AmxCapability DetectAmxCapability()
    {
#if defined(__AMX_TILE__) && defined(__AMX_BF16__)
        return AmxCapability{.tileLoad = true, .tileStore = true, .dpbf16ps = true, .dpbssd = true};
#else
        return AmxCapability{};
#endif
    }

    AmxDispatchPlan PlanAmxOrAvx512Fallback()
    {
        const auto cap = DetectAmxCapability();
        AmxDispatchPlan plan{};
        std::ostringstream reason;
        if (cap.tileLoad && cap.dpbf16ps)
        {
            plan.selectedIsa = AmxPreferredIsa::kAmx;
            plan.tileBytes = 1024U;
            reason << "amx_tile=available;bf16=ok;fallback=none";
        }
        else
        {
            plan.selectedIsa = AmxPreferredIsa::kAvx512;
            plan.tileBytes = 512U;
            reason << "amx_tile=absent;fallback=avx512";
        }
        plan.rationale = reason.str();
        return plan;
    }

} // namespace us4::runtime::backends::cpu_avx
