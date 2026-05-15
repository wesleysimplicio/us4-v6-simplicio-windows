#include "us4/runtime/tuning/auto_tuner.h"

namespace us4::runtime::tuning
{

    std::vector<TuningDecision>
    AutoTuner::BuildPlan(const backends::SessionRequest& request,
                         const backends::HardwareCapabilities& capabilities) const
    {
        std::vector<TuningDecision> plan;
        plan.push_back(TuningDecision{
            .key = "mode",
            .value = request.mode == backends::RuntimeMode::kCpuOnly ? "cpu-only" : "hybrid",
            .rationale = request.mode == backends::RuntimeMode::kCpuOnly
                             ? "Request explicitly asked for CPU-only execution."
                             : "Hybrid mode keeps GPU or NPU paths eligible when available.",
        });
        plan.push_back(TuningDecision{
            .key = "speculative",
            .value = request.enableSpeculative ? "enabled" : "disabled",
            .rationale =
                request.enableSpeculative
                    ? "Speculative decoding is allowed to reduce decode latency."
                    : "Speculative decoding stays off for the simplest deterministic path.",
        });
        plan.push_back(TuningDecision{
            .key = "npu",
            .value = (request.allowNpu && capabilities.hasNpu) ? "enabled" : "disabled",
            .rationale = (request.allowNpu && capabilities.hasNpu)
                             ? "NPU is available and the request opted in."
                             : "NPU is unavailable or was not requested.",
        });
        plan.push_back(TuningDecision{
            .key = "kv-tiering",
            .value = request.maxContextTokens > 4096 ? "enabled" : "disabled",
            .rationale = request.maxContextTokens > 4096
                             ? "Long context benefits from paging KV across memory tiers."
                             : "Short context can stay resident without paging.",
        });
        plan.push_back(TuningDecision{
            .key = "preferred-precision",
            .value = capabilities.hasNpu ? "fp16" : "bf16",
            .rationale = capabilities.hasNpu
                             ? "FP16 is the safest common denominator across GPU and NPU paths."
                             : "BF16 is a better CPU-oriented default when accelerated matrix "
                               "units are available.",
        });
        return plan;
    }

} // namespace us4::runtime::tuning
