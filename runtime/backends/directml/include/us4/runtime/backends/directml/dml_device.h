#pragma once

#include "us4/runtime/backends/directml/dml_types.h"
#include "us4/runtime/backends/hardware_probe.h"

#include <optional>
#include <string>
#include <string_view>

namespace us4::runtime::backends::directml
{

    class DmlDevice
    {
      public:
        explicit DmlDevice(DmlDeviceOptions options = {});

        [[nodiscard]] static bool SupportsSelection(const HardwareCapabilities& capabilities);
        [[nodiscard]] static BackendDescriptor
        BuildDescriptor(const HardwareCapabilities& capabilities);
        [[nodiscard]] static DmlAdapterInfo
        DescribeAdapter(const HardwareCapabilities& capabilities,
                        const DmlDeviceOptions& options = {});

        [[nodiscard]] bool Initialize(const HardwareCapabilities& capabilities);
        [[nodiscard]] bool PrepareGraphSession(std::string_view modelId, DmlExecutionPhase phase,
                                               std::size_t estimatedPersistentBytes);
        void Reset();

        [[nodiscard]] const DmlDeviceOptions& Options() const;
        [[nodiscard]] const DmlAdapterInfo& Adapter() const;
        [[nodiscard]] const BackendDescriptor& Descriptor() const;
        [[nodiscard]] bool IsReady() const;
        [[nodiscard]] std::uint64_t SubmissionFence() const;
        [[nodiscard]] std::optional<DmlIssue> LastIssue() const;
        [[nodiscard]] std::string DescribeSession() const;

      private:
        DmlDeviceOptions options_;
        DmlAdapterInfo adapter_;
        BackendDescriptor descriptor_;
        std::optional<DmlIssue> lastIssue_;
        std::string boundModelId_;
        DmlExecutionPhase boundPhase_ = DmlExecutionPhase::kDecode;
        std::uint64_t submissionFence_ = 0;
        bool ready_ = false;
    };

} // namespace us4::runtime::backends::directml
