#include "us4/runtime/backends/directml/dml_graph.h"

#include <numeric>
#include <sstream>
#include <utility>

namespace us4::runtime::backends::directml
{

    namespace
    {
        constexpr std::size_t kDefaultTemporaryScratchBytes = 8ULL * 1024ULL * 1024ULL;
        constexpr double kCompileCostPerNodeMs = 0.35;
        constexpr double kDispatchCostPerTokenMs = 0.08;
    } // namespace

    DmlGraph::DmlGraph(const DmlDevice* device) : device_(device) {}

    bool DmlGraph::Attach(const DmlDevice* device)
    {
        device_ = device;
        if (device_ == nullptr)
        {
            lastIssue_ = DmlIssue{
                .code = "device-missing",
                .message = "DirectML graph requires an attached device.",
                .recoverable = true,
            };
            state_ = DmlGraphState::kFaulted;
            return false;
        }

        lastIssue_.reset();
        state_ = nodes_.empty() ? DmlGraphState::kEmpty : DmlGraphState::kRecordable;
        return true;
    }

    void DmlGraph::Reset()
    {
        nodes_.clear();
        stats_ = {};
        compileOptions_ = {};
        lastIssue_.reset();
        state_ = DmlGraphState::kEmpty;
    }

    void DmlGraph::RecordNode(DmlGraphNode node)
    {
        nodes_.push_back(std::move(node));
        state_ = DmlGraphState::kRecordable;
    }

    void DmlGraph::RecordNodes(std::vector<DmlGraphNode> nodes)
    {
        for (DmlGraphNode& node : nodes)
        {
            nodes_.push_back(std::move(node));
        }
        state_ = nodes_.empty() ? DmlGraphState::kEmpty : DmlGraphState::kRecordable;
    }

    DmlCompileResult DmlGraph::Compile(const DmlGraphCompileOptions& options)
    {
        compileOptions_ = options;
        lastIssue_.reset();

        if (device_ == nullptr || !device_->IsReady())
        {
            state_ = DmlGraphState::kFaulted;
            lastIssue_ = DmlIssue{
                .code = "device-not-ready",
                .message = "Attach a ready DirectML device before graph compilation.",
                .recoverable = true,
            };
            return DmlCompileResult{
                .state = state_,
                .issue = lastIssue_,
            };
        }

        if (!ValidateRecordSet())
        {
            state_ = DmlGraphState::kFaulted;
            return DmlCompileResult{
                .state = state_,
                .issue = lastIssue_,
            };
        }

        const DmlTensorDataType dataType = ToDmlDataType(options.precision);
        if (!IsPrecisionSupported(device_->Adapter(), dataType))
        {
            state_ = DmlGraphState::kFaulted;
            lastIssue_ = DmlIssue{
                .code = "precision-unsupported",
                .message = "Requested DirectML precision is unsupported by the selected adapter.",
                .recoverable = true,
            };
            return DmlCompileResult{
                .state = state_,
                .issue = lastIssue_,
            };
        }

        const std::size_t estimatedTemporaryBytes = EstimateTemporaryBytes();
        const std::size_t estimatedPersistentBytes = EstimatePersistentBytes();
        const std::size_t temporaryBudget = options.maxTemporaryBytes == 0
                                                ? kDefaultTemporaryScratchBytes
                                                : options.maxTemporaryBytes;
        const std::size_t persistentBudget = options.maxPersistentBytes == 0
                                                 ? device_->Descriptor().budget.deviceBytes
                                                 : options.maxPersistentBytes;
        const bool chunked =
            options.enableChunkedCompilation && (estimatedTemporaryBytes > temporaryBudget ||
                                                 estimatedPersistentBytes > persistentBudget);

        if (!chunked && ((temporaryBudget > 0 && estimatedTemporaryBytes > temporaryBudget) ||
                         (persistentBudget > 0 && estimatedPersistentBytes > persistentBudget)))
        {
            state_ = DmlGraphState::kFaulted;
            lastIssue_ = DmlIssue{
                .code = "compile-budget-exceeded",
                .message = "DirectML graph compile placeholder exceeded configured budgets.",
                .recoverable = true,
            };
            return DmlCompileResult{
                .state = state_,
                .issue = lastIssue_,
            };
        }

        stats_.compileCount += 1;
        stats_.descriptorHeapReuses += options.enablePersistentDescriptors ? 1U : 0U;
        stats_.temporaryBytes = chunked ? temporaryBudget : estimatedTemporaryBytes;
        stats_.persistentBytes =
            chunked ? (estimatedPersistentBytes / 2U) : estimatedPersistentBytes;
        stats_.lastCompileMilliseconds = static_cast<double>(nodes_.size()) * kCompileCostPerNodeMs;
        state_ = DmlGraphState::kCompiled;

        return DmlCompileResult{
            .compiled = true,
            .cacheHit = stats_.compileCount > 1,
            .chunked = chunked,
            .state = state_,
        };
    }

    DmlDispatchResult DmlGraph::Dispatch(const DmlDispatchOptions& options)
    {
        lastIssue_.reset();

        if (state_ != DmlGraphState::kCompiled && state_ != DmlGraphState::kReady)
        {
            state_ = DmlGraphState::kFaulted;
            lastIssue_ = DmlIssue{
                .code = "graph-not-compiled",
                .message = "Compile the DirectML graph before dispatch.",
                .recoverable = true,
            };
            return DmlDispatchResult{
                .state = state_,
                .issue = lastIssue_,
            };
        }

        if (device_ == nullptr || !device_->IsReady())
        {
            state_ = DmlGraphState::kFaulted;
            lastIssue_ = DmlIssue{
                .code = "device-not-ready",
                .message = "DirectML device became unavailable before dispatch.",
                .recoverable = true,
            };
            return DmlDispatchResult{
                .state = state_,
                .issue = lastIssue_,
            };
        }

        stats_.dispatchCount += 1;
        stats_.lastFenceValue = device_->SubmissionFence() + stats_.dispatchCount;
        stats_.lastDispatchMilliseconds =
            static_cast<double>(options.tokenCount * options.batchSize) * kDispatchCostPerTokenMs;
        state_ = DmlGraphState::kReady;

        return DmlDispatchResult{
            .submitted = true,
            .completedSynchronously = !options.allowAsyncCompletion,
            .state = state_,
            .fenceValue = stats_.lastFenceValue,
        };
    }

    bool DmlGraph::IsAttached() const
    {
        return device_ != nullptr;
    }

    DmlGraphState DmlGraph::State() const
    {
        return state_;
    }

    const std::vector<DmlGraphNode>& DmlGraph::Nodes() const
    {
        return nodes_;
    }

    const DmlExecutionStats& DmlGraph::Stats() const
    {
        return stats_;
    }

    std::optional<DmlIssue> DmlGraph::LastIssue() const
    {
        return lastIssue_;
    }

    std::string DmlGraph::DebugSummary() const
    {
        std::ostringstream builder;
        builder << "directml_graph"
                << " state=" << ToString(state_) << " nodes=" << nodes_.size()
                << " compile_count=" << stats_.compileCount
                << " dispatch_count=" << stats_.dispatchCount
                << " temp_bytes=" << stats_.temporaryBytes
                << " persistent_bytes=" << stats_.persistentBytes
                << " last_fence=" << stats_.lastFenceValue;
        return builder.str();
    }

    std::size_t DmlGraph::EstimateTemporaryBytes() const
    {
        return std::accumulate(nodes_.begin(), nodes_.end(), std::size_t{0},
                               [](const std::size_t total, const DmlGraphNode& node)
                               { return total + node.temporaryBytes + (node.inputCount * 256U); });
    }

    std::size_t DmlGraph::EstimatePersistentBytes() const
    {
        return std::accumulate(nodes_.begin(), nodes_.end(), std::size_t{0},
                               [](const std::size_t total, const DmlGraphNode& node)
                               {
                                   return total + (node.usesPersistentWeights
                                                       ? node.persistentBytes
                                                       : (node.persistentBytes / 4U));
                               });
    }

    bool DmlGraph::ValidateRecordSet()
    {
        if (nodes_.empty())
        {
            lastIssue_ = DmlIssue{
                .code = "graph-empty",
                .message = "DirectML graph has no recorded operators to compile.",
                .recoverable = true,
            };
            return false;
        }

        for (const DmlGraphNode& node : nodes_)
        {
            if (node.name.empty())
            {
                lastIssue_ = DmlIssue{
                    .code = "node-name-missing",
                    .message = "DirectML graph nodes require stable names for caching.",
                    .recoverable = true,
                };
                return false;
            }

            if (node.outputCount == 0)
            {
                lastIssue_ = DmlIssue{
                    .code = "node-output-missing",
                    .message = "DirectML graph nodes must expose at least one output.",
                    .recoverable = true,
                };
                return false;
            }
        }

        return true;
    }

} // namespace us4::runtime::backends::directml
