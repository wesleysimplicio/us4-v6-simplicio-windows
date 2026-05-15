#pragma once

#include "us4/runtime/backends/directml/dml_device.h"

#include <optional>
#include <string>
#include <vector>

namespace us4::runtime::backends::directml
{

    class DmlGraph
    {
      public:
        explicit DmlGraph(const DmlDevice* device);

        [[nodiscard]] bool Attach(const DmlDevice* device);
        void Reset();

        void RecordNode(DmlGraphNode node);
        void RecordNodes(std::vector<DmlGraphNode> nodes);

        [[nodiscard]] DmlCompileResult Compile(const DmlGraphCompileOptions& options);
        [[nodiscard]] DmlDispatchResult Dispatch(const DmlDispatchOptions& options);

        [[nodiscard]] bool IsAttached() const;
        [[nodiscard]] DmlGraphState State() const;
        [[nodiscard]] const std::vector<DmlGraphNode>& Nodes() const;
        [[nodiscard]] const DmlExecutionStats& Stats() const;
        [[nodiscard]] std::optional<DmlIssue> LastIssue() const;
        [[nodiscard]] std::string DebugSummary() const;

      private:
        [[nodiscard]] std::size_t EstimateTemporaryBytes() const;
        [[nodiscard]] std::size_t EstimatePersistentBytes() const;
        [[nodiscard]] bool ValidateRecordSet();

        const DmlDevice* device_ = nullptr;
        std::vector<DmlGraphNode> nodes_;
        DmlExecutionStats stats_;
        DmlGraphCompileOptions compileOptions_;
        std::optional<DmlIssue> lastIssue_;
        DmlGraphState state_ = DmlGraphState::kEmpty;
    };

} // namespace us4::runtime::backends::directml
