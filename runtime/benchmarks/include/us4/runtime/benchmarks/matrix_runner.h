#pragma once

#include "us4/runtime/backends/runtime_types.h"
#include "us4/runtime/tuning/auto_tuner.h"

#include <filesystem>
#include <string>
#include <vector>

namespace us4::runtime::benchmarks
{

    struct MatrixSample
    {
        std::string benchmarkName;
        std::string backend;
        std::string profileId;
        double score = 0.0;
        bool supported = false;
        bool regressionCritical = false;
        std::string rationale;
    };

    struct MatrixTuneReport
    {
        tuning::TuningPlan plan;
        std::vector<MatrixSample> samples;
        std::filesystem::path storePath;
        std::string requestedProfileId;
        std::string recommendedProfileId;
        std::string selectedProfileId;
        std::string selectedBackend;
        double selectedScore = 0.0;
        bool persisted = false;
    };

    class MatrixRunner
    {
      public:
        explicit MatrixRunner(std::filesystem::path storePath = {});

        [[nodiscard]] MatrixTuneReport
        Benchmark(const backends::SessionRequest& request,
                  const backends::HardwareCapabilities& capabilities) const;

        [[nodiscard]] MatrixTuneReport
        Tune(const backends::SessionRequest& request,
             const backends::HardwareCapabilities& capabilities) const;

        [[nodiscard]] MatrixTuneReport
        ExecutePlan(const tuning::TuningPlan& plan, const backends::SessionRequest& request,
                    const backends::HardwareCapabilities& capabilities) const;

        [[nodiscard]] static std::string RenderJson(const MatrixTuneReport& report);

      private:
        [[nodiscard]] MatrixTuneReport
        EvaluatePlan(const tuning::TuningPlan& plan, const backends::SessionRequest& request,
                     const backends::HardwareCapabilities& capabilities,
                     bool persistSelection) const;

        std::filesystem::path storePath_;
    };

} // namespace us4::runtime::benchmarks
