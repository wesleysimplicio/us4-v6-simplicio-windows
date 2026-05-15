#pragma once

#include "us4/runtime/backends/hardware_probe.h"
#include "us4/runtime/backends/windows_ml/windows_ml_execution_plan.h"

#include <optional>
#include <string>
#include <string_view>

namespace us4::runtime::backends::windows_ml
{

    enum class WinMlAdapterState
    {
        kCold,
        kReady,
        kCompiled,
        kFaulted,
    };

    struct WinMlAdapterOptions
    {
        bool allowCpuFallback = true;
        bool preferStaticShapes = true;
        bool enableTelemetry = true;
    };

    struct WinMlCompileStats
    {
        std::uint32_t initializeCount = 0;
        std::uint32_t compileCount = 0;
        std::uint32_t npuPartitionCount = 0;
        std::uint32_t hostPartitionCount = 0;
        std::uint32_t cpuFallbackPartitionCount = 0;
        std::uint32_t lastBatchHint = 0;
        std::uint32_t lastContextHint = 0;
    };

    struct WinMlAdapterIssue
    {
        std::string code;
        std::string message;
        bool recoverable = true;
    };

    [[nodiscard]] std::string ToString(WinMlAdapterState state);

    class WinMlAdapter
    {
      public:
        explicit WinMlAdapter(WinMlAdapterOptions options = {});

        [[nodiscard]] bool Initialize(const HardwareCapabilities& capabilities);
        [[nodiscard]] bool CompileGraph(std::string_view modelId,
                                        const WindowsMlExecutionPlan& plan);
        void Reset();

        [[nodiscard]] WinMlAdapterState State() const;
        [[nodiscard]] const WinMlAdapterOptions& Options() const;
        [[nodiscard]] const WinMlCompileStats& Stats() const;
        [[nodiscard]] std::optional<WinMlAdapterIssue> LastIssue() const;
        [[nodiscard]] std::string DescribeSession() const;

      private:
        WinMlAdapterOptions options_;
        WinMlAdapterState state_ = WinMlAdapterState::kCold;
        WinMlCompileStats stats_{};
        std::optional<WinMlAdapterIssue> lastIssue_;
        std::string acceleratorName_;
        std::string boundModelId_;
        bool npuAvailable_ = false;
    };

} // namespace us4::runtime::backends::windows_ml
