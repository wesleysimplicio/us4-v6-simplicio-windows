#pragma once

#include "runtime/core/cpu_scalar_runtime.h"
#include "runtime/core/hardware_probe.h"
#include "runtime/core/runtime_context.h"
#include "runtime/core/runtime_mode.h"
#include "us4/runtime/adapters/adapter_factory.h"
#include "us4/runtime/adapters/model_loader.h"
#include "us4/runtime/backends/cuda/cuda_backend.h"
#include "us4/runtime/backends/directml/dml_device.h"
#include "us4/runtime/backends/directml/dml_graph.h"
#include "us4/runtime/backends/hardware_probe.h"
#include "us4/runtime/backends/vulkan/vulkan_context.h"
#include "us4/runtime/backends/vulkan/vulkan_execution_plan.h"
#include "us4/runtime/backends/windows_ml/layer_offloader.h"
#include "us4/runtime/backends/windows_ml/mixed_dispatch_planner.h"
#include "us4/runtime/backends/windows_ml/power_thermal_monitor.h"
#include "us4/runtime/backends/windows_ml/windows_ml_execution_plan.h"
#include "us4/runtime/backends/windows_ml/winml_adapter.h"
#include "us4/runtime/benchmarks/matrix_runner.h"
#include "us4/runtime/version.h"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>

namespace us4::cli
{

    inline constexpr int kSuccessExitCode = 0;
    inline constexpr int kUsageExitCode = 1;
    inline constexpr int kNotImplementedExitCode = 2;

    enum class CommandKind
    {
        kHelp,
        kVersion,
        kProbe,
        kRun,
        kServe,
        kBench,
        kTune,
        kInvalid,
    };

    struct ParsedCommand
    {
        CommandKind kind = CommandKind::kHelp;
        std::string modelId;
        std::string modelPath;
        std::string prompt;
        std::string backend = "auto";
        bool allowNpu = false;
        std::string mode = "auto";
        std::string format = "text";
        std::uint32_t maxTokens = 32;
        std::string error;
    };

    struct CommandOutput
    {
        int exitCode = kSuccessExitCode;
        std::string stdoutText;
        std::string stderrText;
    };

    inline std::uint32_t EstimatePromptTokens(std::string_view prompt)
    {
        return std::max<std::uint32_t>(1U, static_cast<std::uint32_t>((prompt.size() + 3U) / 4U));
    }

    inline std::string EscapeJson(std::string_view value)
    {
        std::string escaped;
        escaped.reserve(value.size());
        for (const char character : value)
        {
            switch (character)
            {
            case '\\':
                escaped += "\\\\";
                break;
            case '"':
                escaped += "\\\"";
                break;
            case '\n':
                escaped += "\\n";
                break;
            case '\r':
                escaped += "\\r";
                break;
            case '\t':
                escaped += "\\t";
                break;
            default:
                escaped += character;
                break;
            }
        }
        return escaped;
    }

    inline std::string FormatPercent(const float ratio)
    {
        std::ostringstream output;
        output << std::fixed << std::setprecision(2) << (ratio * 100.0F);
        return output.str();
    }

    inline std::string RenderProbeJson(const us4::core::ProbeSummary& summary)
    {
        std::ostringstream json;
        json << "{\n";
        json << "  \"execution\": \"probe\",\n";
        json << "  \"os\": \"" << EscapeJson(summary.osName) << "\",\n";
        json << "  \"cpu\": \"" << EscapeJson(summary.cpuName) << "\",\n";
        json << "  \"gpu\": \"" << EscapeJson(summary.gpuName) << "\",\n";
        json << "  \"has_npu\": " << (summary.hasNpu ? "true" : "false") << ",\n";
        json << "  \"selected_backend\": \"" << EscapeJson(summary.selectedBackend) << "\",\n";
        json << "  \"mode\": \"" << EscapeJson(summary.mode) << "\",\n";
        json << "  \"fallback_backends\": [";
        for (std::size_t index = 0; index < summary.fallbackBackends.size(); ++index)
        {
            if (index > 0U)
            {
                json << ", ";
            }
            json << '"' << EscapeJson(summary.fallbackBackends[index]) << '"';
        }
        json << "],\n";
        json << "  \"moe\": {\n";
        json << "    \"route_count\": " << summary.moeTelemetry.routeCount << ",\n";
        json << "    \"hot_hit_rate_pct\": " << (summary.moeTelemetry.hotHitRate * 100.0F)
             << ",\n";
        json << "    \"warm_hit_rate_pct\": " << (summary.moeTelemetry.warmHitRate * 100.0F)
             << ",\n";
        json << "    \"cold_hit_rate_pct\": " << (summary.moeTelemetry.coldHitRate * 100.0F)
             << ",\n";
        json << "    \"eviction_count\": " << summary.moeTelemetry.evictionCount << ",\n";
        json << "    \"cold_offload_count\": " << summary.moeTelemetry.coldOffloadCount
             << ",\n";
        json << "    \"reload_count\": " << summary.moeTelemetry.reloadCount << ",\n";
        json << "    \"router_entropy\": " << summary.moeTelemetry.routerEntropy << ",\n";
        json << "    \"telemetry_events\": " << summary.moeTelemetry.events.size() << "\n";
        json << "  },\n";
        json << "  \"kv\": {\n";
        json << "    \"segment_count\": " << summary.kvTelemetry.segmentCount << ",\n";
        json << "    \"device_hit_rate_pct\": " << (summary.kvTelemetry.deviceHitRate * 100.0F)
             << ",\n";
        json << "    \"host_hit_rate_pct\": " << (summary.kvTelemetry.hostHitRate * 100.0F)
             << ",\n";
        json << "    \"storage_hit_rate_pct\": " << (summary.kvTelemetry.storageHitRate * 100.0F)
             << ",\n";
        json << "    \"summary_hit_rate_pct\": " << (summary.kvTelemetry.summaryHitRate * 100.0F)
             << ",\n";
        json << "    \"eviction_count\": " << summary.kvTelemetry.evictionCount << ",\n";
        json << "    \"restore_count\": " << summary.kvTelemetry.restoreCount << ",\n";
        json << "    \"summarize_count\": " << summary.kvTelemetry.summarizeCount << ",\n";
        json << "    \"telemetry_events\": " << summary.kvTelemetry.events.size() << "\n";
        json << "  },\n";
        json << "  \"advisories\": [\n";
        for (std::size_t index = 0; index < summary.advisories.size(); ++index)
        {
            const auto& advisory = summary.advisories[index];
            json << "    {\"severity\":\"" << EscapeJson(advisory.severity) << "\",\"code\":\""
                 << EscapeJson(advisory.code) << "\",\"message\":\"" << EscapeJson(advisory.message)
                 << "\"}";
            if (index + 1U < summary.advisories.size())
            {
                json << ',';
            }
            json << '\n';
        }
        json << "  ]\n";
        json << "}\n";
        return json.str();
    }

    inline std::string RenderServeJson(const us4::core::RuntimePlan& plan)
    {
        std::ostringstream json;
        json << "{\n";
        json << "  \"execution\": \"serve\",\n";
        json << "  \"status\": \"scaffold-only\",\n";
        json << "  \"model\": \"" << EscapeJson(plan.model.id) << "\",\n";
        json << "  \"backend\": \"" << EscapeJson(plan.backend.name) << "\",\n";
        json << "  \"mode\": \"" << EscapeJson(us4::core::ToString(plan.mode)) << "\",\n";
        json << "  \"profile\": \"" << EscapeJson(plan.profile.id) << "\",\n";
        json << "  \"allow_npu\": " << (plan.request.allowNpu ? "true" : "false") << ",\n";
        json << "  \"fallback_count\": " << plan.fallbacks.size() << ",\n";
        json << "  \"issue_codes\": [";
        for (std::size_t index = 0; index < plan.issues.size(); ++index)
        {
            if (index > 0U)
            {
                json << ", ";
            }
            json << '"' << EscapeJson(plan.issues[index].code) << '"';
        }
        json << "]\n";
        json << "}\n";
        return json.str();
    }

    inline std::string RenderRunPlanJson(const us4::core::RuntimePlan& plan,
                                         std::string_view prompt, std::string_view executionKind,
                                         std::string_view status, std::string_view reportText)
    {
        std::ostringstream json;
        json << "{\n";
        json << "  \"execution\": \"run\",\n";
        json << "  \"status\": \"" << EscapeJson(status) << "\",\n";
        json << "  \"plan_execution\": \"" << EscapeJson(executionKind) << "\",\n";
        json << "  \"model\": \"" << EscapeJson(plan.model.id) << "\",\n";
        json << "  \"backend\": \"" << EscapeJson(plan.backend.name) << "\",\n";
        json << "  \"mode\": \"" << EscapeJson(us4::core::ToString(plan.mode)) << "\",\n";
        json << "  \"profile\": \"" << EscapeJson(plan.profile.id) << "\",\n";
        json << "  \"allow_npu\": " << (plan.request.allowNpu ? "true" : "false") << ",\n";
        json << "  \"prompt_chars\": " << prompt.size() << ",\n";
        json << "  \"prompt_tokens_estimate\": " << EstimatePromptTokens(prompt) << ",\n";
        json << "  \"issue_codes\": [";
        for (std::size_t index = 0; index < plan.issues.size(); ++index)
        {
            if (index > 0U)
            {
                json << ", ";
            }
            json << '"' << EscapeJson(plan.issues[index].code) << '"';
        }
        json << "],\n";
        json << "  \"report_text\": \"" << EscapeJson(reportText) << "\"\n";
        json << "}\n";
        return json.str();
    }

    inline std::string RenderRunCpuJson(const us4::core::RuntimePlan& plan, std::string_view prompt,
                                        const us4::core::CpuScalarRunResult& runResult,
                                        std::string_view reportText)
    {
        std::ostringstream json;
        json << "{\n";
        json << "  \"execution\": \"run\",\n";
        json << "  \"status\": \"completed\",\n";
        json << "  \"plan_execution\": \"cpu-scalar\",\n";
        json << "  \"model\": \"" << EscapeJson(plan.model.id) << "\",\n";
        json << "  \"family\": \"" << EscapeJson(us4::core::ToString(plan.model.family)) << "\",\n";
        json << "  \"backend\": \"" << EscapeJson(plan.backend.name) << "\",\n";
        json << "  \"mode\": \"" << EscapeJson(us4::core::ToString(plan.mode)) << "\",\n";
        json << "  \"profile\": \"" << EscapeJson(plan.profile.id) << "\",\n";
        json << "  \"supports_moe\": " << (plan.model.supportsMoE ? "true" : "false") << ",\n";
        json << "  \"prompt_chars\": " << prompt.size() << ",\n";
        json << "  \"prompt_tokens_estimate\": " << EstimatePromptTokens(prompt) << ",\n";
        json << "  \"model_path\": \"" << EscapeJson(runResult.report.modelPath) << "\",\n";
        json << "  \"model_format\": \""
             << EscapeJson(us4::runtime::adapters::ToString(runResult.report.modelFormat))
             << "\",\n";
        json << "  \"prefill_tokens\": [";
        for (std::size_t index = 0; index < runResult.report.prefillTokens.size(); ++index)
        {
            if (index > 0U)
            {
                json << ", ";
            }
            json << runResult.report.prefillTokens[index];
        }
        json << "],\n";
        json << "  \"generated_tokens\": [";
        for (std::size_t index = 0; index < runResult.report.generatedTokens.size(); ++index)
        {
            if (index > 0U)
            {
                json << ", ";
            }
            json << runResult.report.generatedTokens[index];
        }
        json << "],\n";
        json << "  \"generated_text\": \"" << EscapeJson(runResult.report.generatedText) << "\",\n";
        json << "  \"reached_eos\": " << (runResult.report.frame.reachedEos ? "true" : "false")
             << ",\n";
        json << "  \"generation\": {\n";
        json << "    \"prefill_tokens\": " << runResult.report.frame.prefillTokens << ",\n";
        json << "    \"decode_tokens\": " << runResult.report.frame.decodeTokens << "\n";
        json << "  },\n";
        json << "  \"kv\": {\n";
        json << "    \"segment_count\": " << runResult.report.kvStats.segmentCount << ",\n";
        json << "    \"pinned_segment_count\": " << runResult.report.kvStats.pinnedSegmentCount
             << ",\n";
        json << "    \"device_hit_rate_pct\": "
             << (runResult.report.kvTierTelemetry.deviceHitRate * 100.0F) << ",\n";
        json << "    \"host_hit_rate_pct\": "
             << (runResult.report.kvTierTelemetry.hostHitRate * 100.0F) << ",\n";
        json << "    \"storage_hit_rate_pct\": "
             << (runResult.report.kvTierTelemetry.storageHitRate * 100.0F) << ",\n";
        json << "    \"summary_hit_rate_pct\": "
             << (runResult.report.kvTierTelemetry.summaryHitRate * 100.0F) << ",\n";
        json << "    \"host_bytes\": " << runResult.report.kvStats.hostBytes << ",\n";
        json << "    \"storage_bytes\": " << runResult.report.kvStats.storageBytes << ",\n";
        json << "    \"summary_bytes\": " << runResult.report.kvStats.summaryBytes << ",\n";
        json << "    \"evictions\": " << runResult.report.kvStats.evictionCount << "\n";
        json << "  },\n";
        json << "  \"prefix_cache\": {\n";
        json << "    \"entries\": " << runResult.report.prefixCacheEntries << ",\n";
        json << "    \"warm_entries\": " << runResult.report.prefixCacheWarmEntries << "\n";
        json << "  },\n";
        json << "  \"multimodal_cache\": {\n";
        json << "    \"entry_count\": " << runResult.report.multimodalCacheTelemetry.entryCount
             << ",\n";
        json << "    \"hit_count\": " << runResult.report.multimodalCacheTelemetry.hitCount
             << ",\n";
        json << "    \"miss_count\": " << runResult.report.multimodalCacheTelemetry.missCount
             << ",\n";
        json << "    \"resident_bytes\": "
             << runResult.report.multimodalCacheTelemetry.residentBytes << ",\n";
        json << "    \"image_entries\": "
             << runResult.report.multimodalCacheTelemetry.imageEntries << ",\n";
        json << "    \"audio_entries\": "
             << runResult.report.multimodalCacheTelemetry.audioEntries << ",\n";
        json << "    \"video_entries\": "
             << runResult.report.multimodalCacheTelemetry.videoEntries << "\n";
        json << "  },\n";
        json << "  \"moe\": {\n";
        json << "    \"route_count\": " << runResult.report.moeRouteCount << ",\n";
        json << "    \"host_route_count\": " << runResult.report.moeHostRouteCount << ",\n";
        json << "    \"router_entropy\": " << runResult.report.moeRouterEntropy << ",\n";
        json << "    \"load_balance_loss\": " << runResult.report.moeLoadBalanceLoss << ",\n";
        json << "    \"hot_experts\": " << runResult.report.moeHotExperts << ",\n";
        json << "    \"warm_experts\": " << runResult.report.moeWarmExperts << ",\n";
        json << "    \"cold_experts\": " << runResult.report.moeColdExperts << ",\n";
        json << "    \"prefetch\": {\n";
        json << "      \"observation_count\": "
             << runResult.report.moePrefetchTelemetry.observationCount << ",\n";
        json << "      \"prediction_count\": "
             << runResult.report.moePrefetchTelemetry.predictionCount << ",\n";
        json << "      \"hit_count\": " << runResult.report.moePrefetchTelemetry.hitCount << ",\n";
        json << "      \"miss_count\": " << runResult.report.moePrefetchTelemetry.missCount << ",\n";
        json << "      \"hit_ratio_pct\": "
             << (runResult.report.moePrefetchTelemetry.hitRatio * 100.0F) << ",\n";
        json << "      \"predicted_experts\": [";
        for (std::size_t index = 0;
             index < runResult.report.moePrefetchTelemetry.predictedExperts.size(); ++index)
        {
            if (index > 0U)
            {
                json << ", ";
            }
            json << runResult.report.moePrefetchTelemetry.predictedExperts[index];
        }
        json << "]\n";
        json << "    },\n";
        json << "    \"sparsity_cache\": {\n";
        json << "      \"entry_count\": " << runResult.report.moeSparsityTelemetry.entryCount
             << ",\n";
        json << "      \"hit_count\": " << runResult.report.moeSparsityTelemetry.hitCount << ",\n";
        json << "      \"miss_count\": " << runResult.report.moeSparsityTelemetry.missCount
             << ",\n";
        json << "      \"evicted_entries\": "
             << runResult.report.moeSparsityTelemetry.evictedEntries << ",\n";
        json << "      \"resident_bytes\": "
             << runResult.report.moeSparsityTelemetry.residentBytes << ",\n";
        json << "      \"average_sparsity_pct\": "
             << (runResult.report.moeSparsityTelemetry.averageSparsity * 100.0F) << ",\n";
        json << "      \"hit_ratio_pct\": "
             << (runResult.report.moeSparsityTelemetry.hitRatio * 100.0F) << "\n";
        json << "    }\n";
        json << "  },\n";
        json << "  \"telemetry\": {\n";
        json << "    \"events\": " << runResult.report.telemetryEventCount << "\n";
        json << "  },\n";
        json << "  \"speculative\": {\n";
        json << "    \"active\": "
             << (runResult.report.speculativeTelemetry.active ? "true" : "false") << ",\n";
        json << "    \"decoder\": \""
             << EscapeJson(runResult.report.speculativeTelemetry.decoder) << "\",\n";
        json << "    \"draft_model_loaded\": "
             << (runResult.report.speculativeTelemetry.draftModelLoaded ? "true" : "false")
             << ",\n";
        json << "    \"draft_model_parameter_count\": "
             << runResult.report.speculativeTelemetry.draftModelParameterCount << ",\n";
        json << "    \"drafted_tokens\": "
             << runResult.report.speculativeTelemetry.draftedTokens << ",\n";
        json << "    \"accepted_tokens\": "
             << runResult.report.speculativeTelemetry.acceptedTokens << ",\n";
        json << "    \"rejected_tokens\": "
             << runResult.report.speculativeTelemetry.rejectedTokens << ",\n";
        json << "    \"acceptance_rate_pct\": "
             << (runResult.report.speculativeTelemetry.acceptanceRate * 100.0F) << ",\n";
        json << "    \"last_step_delta_pct\": "
             << (runResult.report.speculativeTelemetry.lastStepDelta * 100.0F) << ",\n";
        json << "    \"estimated_speedup\": "
             << runResult.report.speculativeTelemetry.estimatedSpeedup << ",\n";
        json << "    \"step_acceptance_rates_pct\": [";
        for (std::size_t index = 0; index < runResult.report.speculativeTelemetry.steps.size();
             ++index)
        {
            if (index > 0U)
            {
                json << ", ";
            }
            json << (runResult.report.speculativeTelemetry.steps[index].acceptanceRate * 100.0F);
        }
        json << "],\n";
        json << "    \"token_acceptance_trace\": [";
        for (std::size_t index = 0;
             index < runResult.report.speculativeTelemetry.tokenAcceptanceTrace.size(); ++index)
        {
            if (index > 0U)
            {
                json << ", ";
            }
            json << runResult.report.speculativeTelemetry.tokenAcceptanceTrace[index];
        }
        json << "]\n";
        json << "  },\n";
        json << "  \"checksums\": {\n";
        json << "    \"scalar_matmul\": " << runResult.report.scalarMatMulChecksum << ",\n";
        json << "    \"scalar_attention\": " << runResult.report.scalarAttentionChecksum << "\n";
        json << "  },\n";
        json << "  \"issue_codes\": [";
        for (std::size_t index = 0; index < plan.issues.size(); ++index)
        {
            if (index > 0U)
            {
                json << ", ";
            }
            json << '"' << EscapeJson(plan.issues[index].code) << '"';
        }
        json << "],\n";
        json << "  \"report_text\": \"" << EscapeJson(reportText) << "\"\n";
        json << "}\n";
        return json.str();
    }

    inline void AppendIssueCodes(std::ostringstream& output, std::string_view prefix,
                                 const std::vector<us4::runtime::backends::RuntimeIssue>& issues)
    {
        if (issues.empty())
        {
            return;
        }

        output << prefix << ".issue_codes: ";
        for (std::size_t index = 0; index < issues.size(); ++index)
        {
            if (index > 0U)
            {
                output << ',';
            }
            output << issues[index].code;
        }
        output << '\n';
    }

    inline void AppendAdapterDryRun(std::ostringstream& output, std::string_view prefix,
                                    const us4::core::RuntimePlan& plan, std::string_view prompt,
                                    std::string_view modelPath)
    {
        const std::string resolvedModelPath =
            modelPath.empty() ? "builtin://" + plan.model.id : std::string(modelPath);
        const auto adapter = us4::runtime::adapters::CreateAdapterForModelId(plan.model.id);
        output << prefix << ".adapter_id: " << plan.model.adapterId << '\n';
        output << prefix << ".model_path: " << resolvedModelPath << '\n';

        const auto asset =
            us4::runtime::adapters::LoadModelAsset(resolvedModelPath, plan.model.id);
        output << prefix << ".model_format: "
               << us4::runtime::adapters::ToString(asset.descriptor.format) << '\n';

        if (!adapter)
        {
            output << prefix << ".adapter_created: no\n";
            return;
        }

        output << prefix << ".adapter_created: yes\n";
        const bool attached = adapter->Attach(plan.binding);
        output << prefix << ".adapter_attached: " << (attached ? "yes" : "no") << '\n';

        if (!attached)
        {
            return;
        }

        const bool loaded = adapter->LoadModel(resolvedModelPath);
        output << prefix << ".adapter_model_loaded: " << (loaded ? "yes" : "no") << '\n';

        const auto residency = adapter->BuildResidencyPlan(plan.request);
        output << prefix << ".adapter_expected_host_bytes: " << residency.expectedHostBytes << '\n';
        output << prefix << ".adapter_expected_device_bytes: " << residency.expectedDeviceBytes
               << '\n';

        if (!loaded)
        {
            return;
        }

        const auto prefill = adapter->Prefill(std::string(prompt));
        output << prefix << ".adapter_prefill_tokens: " << prefill.tokens.size() << '\n';
        output << prefix << ".adapter_prefill_terminal: " << (prefill.isTerminal ? "yes" : "no")
               << '\n';
    }

    inline void AppendCudaDryRun(std::ostringstream& output, const us4::core::RuntimePlan& plan,
                                 const us4::runtime::backends::HardwareCapabilities& capabilities,
                                 std::string_view prompt, std::string_view modelPath)
    {
        const auto executionPlan = us4::runtime::backends::cuda::CudaBackend::BuildExecutionPlan(
            plan.request, capabilities);
        const auto prefill =
            us4::runtime::backends::cuda::CudaBackend::PreparePrefill(executionPlan, prompt);
        const auto decode = us4::runtime::backends::cuda::CudaBackend::PrepareDecode(
            executionPlan, prefill.chunk, 0U);

        output << "execution: cuda-dry-run\n";
        output << "cuda.architecture: "
               << us4::runtime::backends::cuda::ToString(executionPlan.device.architecture) << '\n';
        output << "cuda.compute_capability: " << executionPlan.device.computeCapabilityMajor << '.'
               << executionPlan.device.computeCapabilityMinor << '\n';
        output << "cuda.execution_flavor: "
               << us4::runtime::backends::cuda::ToString(executionPlan.flavor) << '\n';
        output << "cuda.residency: "
               << us4::runtime::backends::cuda::ToString(executionPlan.memory.residency) << '\n';
        output << "cuda.prefill_streams: " << executionPlan.streams.prefillStreams << '\n';
        output << "cuda.decode_streams: " << executionPlan.streams.decodeStreams << '\n';
        output << "cuda.graph_capture: " << (executionPlan.graph.enableGraphCapture ? "yes" : "no")
               << '\n';
        output << "cuda.prefill_chunk_tokens: " << prefill.chunk.tokens.size() << '\n';
        output << "cuda.decode_preview_tokens: " << decode.chunk.tokens.size() << '\n';
        output << "cuda.prefill_bytes_touched: " << prefill.deviceBytesTouched << '\n';
        output << "cuda.decode_bytes_touched: " << decode.deviceBytesTouched << '\n';
        output << "cuda.plan_issues: " << executionPlan.issues.size() << '\n';
        AppendAdapterDryRun(output, "cuda", plan, prompt, modelPath);
        AppendIssueCodes(output, "cuda", executionPlan.issues);
    }

    inline void
    AppendDirectMlDryRun(std::ostringstream& output, const us4::core::RuntimePlan& plan,
                         const us4::runtime::backends::HardwareCapabilities& capabilities,
                         std::string_view prompt, std::string_view modelPath)
    {
        using namespace us4::runtime::backends::directml;

        DmlDevice device({
            .preferIntegratedGpu =
                plan.backend.deviceClass == us4::runtime::backends::DeviceClass::kIntegratedGpu,
            .preferLowLatency = plan.request.preferLowLatency,
        });
        const bool initialized = device.Initialize(capabilities);
        const std::size_t persistentBytes =
            std::max<std::size_t>(plan.residency.expectedDeviceBytes / 4U, 1024U * 1024U);
        const bool prepared =
            initialized &&
            device.PrepareGraphSession(plan.model.id, DmlExecutionPhase::kPrefill, persistentBytes);

        DmlGraph graph(&device);
        graph.RecordNode({
            .name = "prefill.matmul",
            .kind = DmlOperatorKind::kMatMul,
            .dataType = ToDmlDataType(plan.backend.defaultPrecision),
            .inputCount = 2,
            .outputCount = 1,
            .usesPersistentWeights = true,
            .allowChunking = true,
            .temporaryBytes =
                std::max<std::size_t>(plan.residency.expectedPrefillScratchBytes, 512U * 1024U),
            .persistentBytes = persistentBytes / 2U,
        });
        graph.RecordNode({
            .name = "decode.attention",
            .kind = DmlOperatorKind::kAttention,
            .dataType = ToDmlDataType(plan.backend.defaultPrecision),
            .inputCount = 3,
            .outputCount = 1,
            .usesPersistentWeights = false,
            .allowChunking = true,
            .temporaryBytes =
                std::max<std::size_t>(plan.residency.expectedDecodeScratchBytes, 256U * 1024U),
            .persistentBytes = persistentBytes / 4U,
        });

        const auto compile = graph.Compile({
            .precision = plan.backend.defaultPrecision,
            .enableOperatorFusion = true,
            .enablePersistentDescriptors = true,
            .enableChunkedCompilation = true,
            .maxTemporaryBytes =
                std::max<std::size_t>(plan.residency.expectedPrefillScratchBytes, 1024U * 1024U),
            .maxPersistentBytes = persistentBytes,
        });
        const auto dispatch = graph.Dispatch({
            .phase = DmlExecutionPhase::kPrefill,
            .tokenCount = EstimatePromptTokens(prompt),
            .batchSize =
                static_cast<std::uint32_t>(std::max<std::size_t>(plan.profile.targetBatchSize, 1U)),
            .sequenceLength = std::max<std::uint32_t>(EstimatePromptTokens(prompt), 1U),
            .allowAsyncCompletion = true,
        });

        output << "execution: directml-dry-run\n";
        output << "directml.initialized: " << (initialized ? "yes" : "no") << '\n';
        output << "directml.session_prepared: " << (prepared ? "yes" : "no") << '\n';
        output << "directml.graph_state: " << ToString(graph.State()) << '\n';
        output << "directml.compile_ok: " << (compile.compiled ? "yes" : "no") << '\n';
        output << "directml.dispatch_ok: " << (dispatch.submitted ? "yes" : "no") << '\n';
        output << "directml.node_count: " << graph.Nodes().size() << '\n';
        output << "directml.dispatch_count: " << graph.Stats().dispatchCount << '\n';
        output << "directml.fence: " << graph.Stats().lastFenceValue << '\n';
        AppendAdapterDryRun(output, "directml", plan, prompt, modelPath);
    }

    inline void AppendVulkanDryRun(std::ostringstream& output, const us4::core::RuntimePlan& plan,
                                   const us4::runtime::backends::HardwareCapabilities& capabilities)
    {
        const auto executionPlan = us4::runtime::backends::vulkan::BuildVulkanExecutionPlan({
            .binding = plan.binding,
            .request = plan.request,
            .adapterId = plan.model.adapterId,
            .targetBatchSize = plan.profile.targetBatchSize,
            .modelSupportsMoE = plan.model.supportsMoE,
            .modelSupportsSpeculative = plan.model.supportsSpeculative,
        });
        us4::runtime::backends::vulkan::VulkanContext context({
            .preferIntegratedGpu =
                plan.backend.deviceClass == us4::runtime::backends::DeviceClass::kIntegratedGpu,
            .enableValidationLayer = false,
            .allowDescriptorBuffer = true,
            .preferAsyncTransfers = true,
        });
        const bool initialized = context.Initialize(capabilities);
        const bool bound = initialized && context.BindExecutionPlan(plan.model.id, executionPlan);

        output << "execution: vulkan-dry-run\n";
        output << "vulkan.context_initialized: " << (initialized ? "yes" : "no") << '\n';
        output << "vulkan.context_bound: " << (bound ? "yes" : "no") << '\n';
        output << "vulkan.context_state: "
               << us4::runtime::backends::vulkan::ToString(context.State()) << '\n';
        output << "vulkan.step_count: " << executionPlan.steps.size() << '\n';
        output << "vulkan.decode_micro_batch: " << executionPlan.decodeMicroBatchSize << '\n';
        output << "vulkan.prefill_batches: " << executionPlan.maxConcurrentPrefillBatches << '\n';
        output << "vulkan.descriptor_sets: " << context.DescriptorArena().setCount << '\n';
        output << "vulkan.persistent_bytes: " << context.DescriptorArena().persistentBytes << '\n';
        output << "vulkan.transient_bytes: " << context.DescriptorArena().transientBytes << '\n';
        output << "vulkan.kernel_manifest_loaded: "
               << (context.KernelLibrary().Loaded() ? "yes" : "no") << '\n';
        output << "vulkan.kernel_count: " << context.Stats().loadedKernelCount << '\n';
        output << "vulkan.required_kernel_count: " << context.Stats().requiredKernelCount << '\n';
        output << "vulkan.timeline_semaphores: "
               << (executionPlan.useTimelineSemaphores ? "yes" : "no") << '\n';
        output << "vulkan.cpu_attention_fallback: "
               << (executionPlan.fallbackToCpuAttention ? "yes" : "no") << '\n';
        output << "vulkan.host_moe_route: " << (executionPlan.routeMoEOnHost ? "yes" : "no")
               << '\n';
        output << "vulkan.plan_issues: " << executionPlan.issues.size() << '\n';
        AppendIssueCodes(output, "vulkan", executionPlan.issues);
        if (!executionPlan.steps.empty())
        {
            output << "vulkan.first_stage: "
                   << us4::runtime::backends::vulkan::ToString(executionPlan.steps.front().stage)
                   << '\n';
            output << "vulkan.last_stage: "
                   << us4::runtime::backends::vulkan::ToString(executionPlan.steps.back().stage)
                   << '\n';
        }
    }

    inline void
    AppendWindowsMlDryRun(std::ostringstream& output, const us4::core::RuntimePlan& plan,
                          const us4::runtime::backends::HardwareCapabilities& capabilities)
    {
        const auto executionPlan = us4::runtime::backends::windows_ml::BuildWindowsMlExecutionPlan({
            .binding = plan.binding,
            .request = plan.request,
            .adapterId = plan.model.adapterId,
            .targetBatchSize = plan.profile.targetBatchSize,
            .modelSupportsMoE = plan.model.supportsMoE,
            .modelSupportsSpeculative = plan.model.supportsSpeculative,
            .modelSupportsVision = plan.model.supportsVision,
        });
        us4::runtime::backends::windows_ml::WinMlAdapter adapter({
            .allowCpuFallback = true,
            .preferStaticShapes = true,
            .enableTelemetry = true,
        });
        const std::vector<us4::runtime::backends::windows_ml::LayerDescriptor> sampleLayers = {
            {
                .name = "embed",
                .kind = us4::runtime::backends::windows_ml::LayerKind::kEmbedding,
            },
            {
                .name = "prefill.ffn",
                .kind = us4::runtime::backends::windows_ml::LayerKind::kDensePrefill,
            },
            {
                .name = "decode.ffn",
                .kind = us4::runtime::backends::windows_ml::LayerKind::kDenseDecode,
                .latencySensitive = true,
            },
            {
                .name = "attention",
                .kind = us4::runtime::backends::windows_ml::LayerKind::kAttention,
            },
            {
                .name = "kv-compress",
                .kind = us4::runtime::backends::windows_ml::LayerKind::kKvCompression,
            },
        };
        const bool initialized = adapter.Initialize(capabilities);
        const bool compiled = initialized && adapter.CompileGraph(plan.model.id, executionPlan);
        const auto powerSnapshot =
            us4::runtime::backends::windows_ml::PowerThermalMonitor::Sample();
        const auto powerIssues =
            us4::runtime::backends::windows_ml::PowerThermalMonitor::BuildIssues(powerSnapshot);
        const auto dispatchTable =
            us4::runtime::backends::windows_ml::LayerOffloader::BuildDispatchTable(executionPlan,
                                                                                   sampleLayers);

        output << "execution: windows-ml-dry-run\n";
        output << "windows_ml.adapter_initialized: " << (initialized ? "yes" : "no") << '\n';
        output << "windows_ml.graph_compiled: " << (compiled ? "yes" : "no") << '\n';
        output << "windows_ml.adapter_state: "
               << us4::runtime::backends::windows_ml::ToString(adapter.State()) << '\n';
        output << "windows_ml.opt_in_satisfied: " << (executionPlan.optInSatisfied ? "yes" : "no")
               << '\n';
        output << "windows_ml.prefill_offload: " << (executionPlan.offloadPrefill ? "yes" : "no")
               << '\n';
        output << "windows_ml.decode_offload: " << (executionPlan.offloadDecode ? "yes" : "no")
               << '\n';
        output << "windows_ml.kv_host_compression: "
               << (executionPlan.compressKvOnHost ? "yes" : "no") << '\n';
        output << "windows_ml.partition_count: " << executionPlan.partitions.size() << '\n';
        output << "windows_ml.npu_partitions: " << adapter.Stats().npuPartitionCount << '\n';
        output << "windows_ml.host_partitions: " << adapter.Stats().hostPartitionCount << '\n';
        output << "windows_ml.cpu_fallback_partitions: "
               << adapter.Stats().cpuFallbackPartitionCount << '\n';
        output << "windows_ml.dispatch_table_size: " << dispatchTable.size() << '\n';
        output << "windows_ml.batch_hint: " << executionPlan.batchSizeHint << '\n';
        output << "windows_ml.context_hint: " << executionPlan.contextTokenHint << '\n';
        output << "windows_ml.plan_issues: " << executionPlan.issues.size() << '\n';
        const auto sessionArtifact = adapter.SessionArtifact();
        if (sessionArtifact.has_value())
        {
            output << "windows_ml.compile_target: "
                   << us4::runtime::backends::windows_ml::ToString(sessionArtifact->compileTarget)
                   << '\n';
            output << "windows_ml.cpu_fallback_armed: "
                   << (sessionArtifact->cpuFallbackArmed ? "yes" : "no") << '\n';
            output << "windows_ml.graph_reusable: "
                   << (sessionArtifact->reusableGraph ? "yes" : "no") << '\n';
            output << "windows_ml.requires_static_shapes: "
                   << (sessionArtifact->requiresStaticShapes ? "yes" : "no") << '\n';
            output << "windows_ml.session_artifact: " << sessionArtifact->cacheKey << '\n';
            output << "windows_ml.host_assist_required: "
                   << (sessionArtifact->hostAssistRequired ? "yes" : "no") << '\n';
            if (!sessionArtifact->fallbackReason.empty())
            {
                output << "windows_ml.fallback_reason: " << sessionArtifact->fallbackReason << '\n';
            }
        }
        output << "windows_ml.power_source: "
               << us4::runtime::backends::windows_ml::ToString(powerSnapshot.powerSource) << '\n';
        output << "windows_ml.battery_percent: " << powerSnapshot.batteryPercent << '\n';
        output << "windows_ml.battery_saver: " << (powerSnapshot.batterySaverActive ? "yes" : "no")
               << '\n';
        output << "windows_ml.thermal_state: "
               << us4::runtime::backends::windows_ml::ToString(powerSnapshot.thermalState) << '\n';
        output << "windows_ml.etw_throttled: " << (powerSnapshot.etwThrottleActive ? "yes" : "no")
               << '\n';
        output << "windows_ml.power_policy: "
               << us4::runtime::backends::windows_ml::ToString(
                      us4::runtime::backends::windows_ml::PowerThermalMonitor::SelectPolicy(
                          powerSnapshot))
               << '\n';
        output << "windows_ml.synthetic_power_telemetry: "
               << (powerSnapshot.usingSyntheticTelemetry ? "yes" : "no") << '\n';
        AppendIssueCodes(output, "windows_ml", executionPlan.issues);
        AppendIssueCodes(output, "windows_ml.power", powerIssues);
        if (!dispatchTable.empty())
        {
            output << "windows_ml.first_dispatch_target: "
                   << us4::runtime::backends::windows_ml::ToString(dispatchTable.front().target)
                   << '\n';
            output << "windows_ml.last_dispatch_target: "
                   << us4::runtime::backends::windows_ml::ToString(dispatchTable.back().target)
                   << '\n';
        }
        if (!executionPlan.partitions.empty())
        {
            output << "windows_ml.last_partition: "
                   << us4::runtime::backends::windows_ml::ToString(
                          executionPlan.partitions.back().kind)
                   << '\n';
        }

        output << "windows_ml.mixed_dispatch_active: " << (capabilities.hasVulkan ? "yes" : "no")
               << '\n';
        if (capabilities.hasVulkan)
        {
            auto gpuBinding = plan.binding;
            gpuBinding.backend = us4::runtime::backends::BackendDescriptor{
                .kind = us4::runtime::backends::BackendKind::kVulkan,
                .name = "vulkan",
                .displayName = "Vulkan Compute",
                .deviceClass = capabilities.primaryAdapterClass,
                .vendor = capabilities.primaryAdapterVendor,
                .availability = us4::runtime::backends::BackendAvailability::kAvailable,
                .defaultPrecision = us4::runtime::backends::PrecisionMode::kFp16,
                .maxContextTokensHint = std::max<std::uint32_t>(
                    plan.request.maxContextTokens, plan.binding.backend.maxContextTokensHint == 0
                                                       ? 4096U
                                                       : plan.binding.backend.maxContextTokensHint),
                .supportsPagedKv = true,
                .supportsSpeculative = true,
                .supportsUnifiedMemory = capabilities.hasUnifiedMemory ||
                                         capabilities.primaryAdapterClass ==
                                             us4::runtime::backends::DeviceClass::kIntegratedGpu,
                .budget = capabilities.budget,
            };
            const auto gpuPlan = us4::runtime::backends::vulkan::BuildVulkanExecutionPlan({
                .binding = gpuBinding,
                .request = plan.request,
                .adapterId = plan.model.adapterId,
                .targetBatchSize = plan.profile.targetBatchSize,
                .modelSupportsMoE = plan.model.supportsMoE,
                .modelSupportsSpeculative = plan.model.supportsSpeculative,
            });
            const auto mixedDispatchPlan =
                us4::runtime::backends::windows_ml::MixedDispatchPlanner::Build(
                    gpuPlan, executionPlan, sampleLayers, powerSnapshot,
                    sessionArtifact ? &*sessionArtifact : nullptr);
            output << "windows_ml.mixed_dispatch_slice_count: " << mixedDispatchPlan.slices.size()
                   << '\n';
            output << "windows_ml.mixed_dispatch_gpu_primary: "
                   << (mixedDispatchPlan.gpuPrimaryActive ? "yes" : "no") << '\n';
            output << "windows_ml.mixed_dispatch_npu_dense: "
                   << (mixedDispatchPlan.npuDenseActive ? "yes" : "no") << '\n';
            output << "windows_ml.mixed_dispatch_host_assist: "
                   << (mixedDispatchPlan.hostAssistRequired ? "yes" : "no") << '\n';
            output << "windows_ml.mixed_dispatch_cpu_fallback: "
                   << (mixedDispatchPlan.cpuFallbackPresent ? "yes" : "no") << '\n';
            output << "windows_ml.mixed_dispatch_policy: "
                   << us4::runtime::backends::windows_ml::ToString(mixedDispatchPlan.policy)
                   << '\n';
            output << "windows_ml.mixed_dispatch_policy_degraded: "
                   << (mixedDispatchPlan.degradedByPolicy ? "yes" : "no") << '\n';
            output << "windows_ml.mixed_dispatch_npu_demotions: "
                   << mixedDispatchPlan.npuDemotionCount << '\n';
            if (!mixedDispatchPlan.slices.empty())
            {
                output << "windows_ml.mixed_dispatch_first_target: "
                       << us4::runtime::backends::windows_ml::ToString(
                              mixedDispatchPlan.slices.front().target)
                       << '\n';
                output << "windows_ml.mixed_dispatch_last_target: "
                       << us4::runtime::backends::windows_ml::ToString(
                              mixedDispatchPlan.slices.back().target)
                       << '\n';
            }
        }
    }

    inline std::string RenderHelp()
    {
        std::ostringstream output;
        output << "us4-cli\n"
               << "Usage:\n"
               << "  us4-cli help\n"
               << "  us4-cli version\n"
               << "  us4-cli probe [--format <text|json>]\n"
               << "  us4-cli run --model <model-id> --prompt <text> [--model-path <asset>] "
                  "[--format <text|json>] "
                  "[--max-tokens <count>] [--backend "
                  "<auto|cpu|cuda|directml|vulkan|windows-ml|npu>] [--mode "
                  "<auto|full|balanced|degraded|ultra_low|micro|nano|cpu_only>] [--npu]\n"
               << "  us4-cli serve --model <model-id> [--format <text|json>] [--backend "
                  "<auto|cpu|cuda|directml|vulkan|windows-ml|npu>] [--mode "
                  "<auto|full|balanced|degraded|ultra_low|micro|nano|cpu_only>] [--npu]\n"
               << "  us4-cli bench --model <model-id> [--format <text|json>] [--max-tokens "
                  "<count>] [--backend "
                  "<auto|cpu|cuda|directml|vulkan|windows-ml|npu>] [--mode "
                  "<auto|full|balanced|degraded|ultra_low|micro|nano|cpu_only>] [--npu]\n"
               << "  us4-cli tune --model <model-id> [--format <text|json>] [--max-tokens "
                  "<count>] [--backend "
                  "<auto|cpu|cuda|directml|vulkan|windows-ml|npu>] [--mode "
                  "<auto|full|balanced|degraded|ultra_low|micro|nano|cpu_only>] [--npu]\n"
               << "\n"
               << "Commands:\n"
               << "  help, --help, -h    Show this help\n"
               << "  version, --version  Show CLI version\n"
               << "  probe, --probe      Print detected hardware/backend summary\n"
               << "  serve               Render the serving plan scaffold for the current host\n"
               << "  run                 Execute CPU_ONLY baseline or print the scaffolded "
                  "execution plan\n"
               << "  bench               Execute the matrix planner and export the current "
                  "benchmark matrix\n"
               << "  tune                Execute the mini-bench planner and persist the best "
                  "profile for this host\n"
               << "\n"
               << "Examples:\n"
               << "  us4-cli probe\n"
               << "  us4-cli version\n"
               << "  us4-cli serve --model qwen-0.5b --backend windows-ml --format json\n"
               << "  us4-cli run --model qwen-0.5b --prompt \"hi\" --backend cpu\n"
               << "  us4-cli bench --model qwen-0.5b --format json\n"
               << "  us4-cli tune --model qwen-0.5b --backend windows-ml --npu\n";
        return output.str();
    }

    inline ParsedCommand ParseArguments(const int argc, char** argv)
    {
        if (argc <= 1)
        {
            return {};
        }

        ParsedCommand command{};
        const std::string_view verb = argv[1];

        if (verb == "--help" || verb == "-h" || verb == "help")
        {
            return command;
        }

        if (verb == "--version" || verb == "version")
        {
            command.kind = CommandKind::kVersion;
            return command;
        }

        if (verb != "--probe" && verb != "probe" && verb != "run" && verb != "serve" &&
            verb != "bench" && verb != "tune")
        {
            command.kind = CommandKind::kInvalid;
            command.error = "Unknown command: " + std::string(verb);
            return command;
        }

        if (verb == "--probe" || verb == "probe")
        {
            command.kind = CommandKind::kProbe;
        }
        else if (verb == "run")
        {
            command.kind = CommandKind::kRun;
        }
        else if (verb == "serve")
        {
            command.kind = CommandKind::kServe;
        }
        else if (verb == "bench")
        {
            command.kind = CommandKind::kBench;
        }
        else
        {
            command.kind = CommandKind::kTune;
        }
        for (int index = 2; index < argc; ++index)
        {
            const std::string_view option = argv[index];
            if (option == "--model")
            {
                if (index + 1 >= argc)
                {
                    command.kind = CommandKind::kInvalid;
                    command.error = "Missing value for --model.";
                    return command;
                }
                command.modelId = argv[++index];
                continue;
            }
            if (option == "--prompt")
            {
                if (index + 1 >= argc)
                {
                    command.kind = CommandKind::kInvalid;
                    command.error = "Missing value for --prompt.";
                    return command;
                }
                command.prompt = argv[++index];
                continue;
            }
            if (option == "--model-path")
            {
                if (index + 1 >= argc)
                {
                    command.kind = CommandKind::kInvalid;
                    command.error = "Missing value for --model-path.";
                    return command;
                }
                command.modelPath = argv[++index];
                continue;
            }
            if (option == "--mode")
            {
                if (index + 1 >= argc)
                {
                    command.kind = CommandKind::kInvalid;
                    command.error = "Missing value for --mode.";
                    return command;
                }
                command.mode = argv[++index];
                continue;
            }
            if (option == "--backend")
            {
                if (index + 1 >= argc)
                {
                    command.kind = CommandKind::kInvalid;
                    command.error = "Missing value for --backend.";
                    return command;
                }
                command.backend = argv[++index];
                continue;
            }
            if (option == "--max-tokens")
            {
                if (index + 1 >= argc)
                {
                    command.kind = CommandKind::kInvalid;
                    command.error = "Missing value for --max-tokens.";
                    return command;
                }

                std::uint32_t parsedValue = 0;
                const std::string_view rawValue = argv[++index];
                const auto [ptr, ec] = std::from_chars(
                    rawValue.data(), rawValue.data() + rawValue.size(), parsedValue);
                if (ec != std::errc{} || ptr != rawValue.data() + rawValue.size() ||
                    parsedValue == 0U)
                {
                    command.kind = CommandKind::kInvalid;
                    command.error = "The --max-tokens value must be a positive integer.";
                    return command;
                }
                command.maxTokens = parsedValue;
                continue;
            }
            if (option == "--format")
            {
                if (index + 1 >= argc)
                {
                    command.kind = CommandKind::kInvalid;
                    command.error = "Missing value for --format.";
                    return command;
                }
                command.format = argv[++index];
                continue;
            }
            if (option == "--npu")
            {
                command.allowNpu = true;
                continue;
            }

            command.kind = CommandKind::kInvalid;
            command.error = "Unknown CLI option: " + std::string(option);
            return command;
        }

        if ((command.kind == CommandKind::kRun || command.kind == CommandKind::kServe ||
             command.kind == CommandKind::kBench || command.kind == CommandKind::kTune) &&
            command.modelId.empty())
        {
            command.kind = CommandKind::kInvalid;
            command.error = "The command requires --model <model-id>.";
            return command;
        }
        if (command.kind == CommandKind::kRun && command.prompt.empty())
        {
            command.kind = CommandKind::kInvalid;
            command.error = "The run command requires --prompt <text>.";
            return command;
        }

        return command;
    }

    inline CommandOutput ExecuteCommand(const ParsedCommand& command)
    {
        const auto isAutoMode = [](std::string value)
        {
            std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character)
                           { return static_cast<char>(std::tolower(character)); });
            return value == "auto";
        };
        const auto normalizeBackend = [](std::string value)
        {
            std::transform(value.begin(), value.end(), value.begin(),
                           [](unsigned char character)
                           {
                               if (character == '_' || character == ' ')
                               {
                                   return '-';
                               }
                               return static_cast<char>(std::tolower(character));
                           });
            return value;
        };
        const auto normalizeFormat = [](std::string value)
        {
            std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character)
                           { return static_cast<char>(std::tolower(character)); });
            return value;
        };

        if (command.kind == CommandKind::kInvalid)
        {
            return CommandOutput{
                kUsageExitCode,
                RenderHelp(),
                command.error + '\n',
            };
        }

        if (command.kind == CommandKind::kHelp)
        {
            return CommandOutput{
                kSuccessExitCode,
                RenderHelp(),
                {},
            };
        }

        if (command.kind == CommandKind::kVersion)
        {
            return CommandOutput{
                kSuccessExitCode,
                "us4-cli " + std::string(us4::runtime::version::kVersion) + "\n",
                {},
            };
        }

        const std::string normalizedFormat = normalizeFormat(command.format);
        if (normalizedFormat != "text" && normalizedFormat != "json")
        {
            return CommandOutput{
                kUsageExitCode,
                RenderHelp(),
                "Unknown value for --format.\n",
            };
        }

        if (command.kind == CommandKind::kProbe)
        {
            const auto summary = us4::core::ProbeHardware();
            return CommandOutput{
                kSuccessExitCode,
                normalizedFormat == "json" ? RenderProbeJson(summary)
                                           : us4::core::FormatProbeSummary(summary),
                {},
            };
        }

        us4::runtime::backends::SessionRequest request{};
        request.modelId = command.modelId;
        request.preferredBackend = normalizeBackend(command.backend);
        request.allowNpu = command.allowNpu;
        request.maxGenerationTokens = command.maxTokens;

        if (request.preferredBackend != "auto" && request.preferredBackend != "cpu" &&
            request.preferredBackend != "cuda" && request.preferredBackend != "directml" &&
            request.preferredBackend != "vulkan" && request.preferredBackend != "windows-ml" &&
            request.preferredBackend != "npu")
        {
            return CommandOutput{
                kUsageExitCode,
                RenderHelp(),
                "Unknown value for --backend.\n",
            };
        }

        const auto parsedMode = us4::core::ParseRuntimeMode(command.mode);
        if (!parsedMode.has_value() && !isAutoMode(command.mode))
        {
            return CommandOutput{
                kUsageExitCode,
                RenderHelp(),
                "Unknown value for --mode.\n",
            };
        }
        if (parsedMode.has_value())
        {
            request.mode = *parsedMode;
        }
        else if (request.preferredBackend == "cpu")
        {
            request.mode = us4::runtime::backends::RuntimeMode::kCpuOnly;
        }

        const us4::runtime::backends::HardwareCapabilities capabilities =
            us4::runtime::backends::HardwareProbe::DetectHost();

        if (command.kind == CommandKind::kTune)
        {
            const us4::runtime::benchmarks::MatrixRunner runner;
            const auto tuneReport = runner.Tune(request, capabilities);
            if (normalizedFormat == "json")
            {
                return CommandOutput{
                    kSuccessExitCode,
                    us4::runtime::benchmarks::MatrixRunner::RenderJson(tuneReport, "tune"),
                    {},
                };
            }

            const us4::core::RuntimePlan tunedPlan =
                us4::core::RuntimeContext::BuildPlan(request, capabilities);

            std::ostringstream output;
            output << us4::core::FormatRuntimePlan(tunedPlan);
            output << "execution: tune\n";
            output << "tune.requested_profile: " << tuneReport.requestedProfileId << '\n';
            output << "tune.recommended_profile: " << tuneReport.recommendedProfileId << '\n';
            output << "tune.selected_profile: " << tuneReport.selectedProfileId << '\n';
            output << "tune.selected_backend: " << tuneReport.selectedBackend << '\n';
            output << "tune.selected_score: " << tuneReport.selectedScore << '\n';
            output << "tune.store_path: " << tuneReport.storePath.string() << '\n';
            output << "tune.persisted: " << (tuneReport.persisted ? "yes" : "no") << '\n';
            output << "tune.sample_count: " << tuneReport.samples.size() << '\n';
            for (std::size_t index = 0; index < tuneReport.samples.size(); ++index)
            {
                const auto& sample = tuneReport.samples[index];
                const std::size_t sampleNumber = index + 1U;
                output << "tune.sample_" << sampleNumber << ".benchmark: " << sample.benchmarkName
                       << '\n';
                output << "tune.sample_" << sampleNumber << ".backend: " << sample.backend << '\n';
                output << "tune.sample_" << sampleNumber << ".profile: " << sample.profileId
                       << '\n';
                output << "tune.sample_" << sampleNumber
                       << ".supported: " << (sample.supported ? "yes" : "no") << '\n';
                output << "tune.sample_" << sampleNumber << ".score: " << sample.score << '\n';
            }
            output << "tune_status: completed\n";

            return CommandOutput{
                kSuccessExitCode,
                output.str(),
                {},
            };
        }

        if (command.kind == CommandKind::kBench)
        {
            const us4::runtime::benchmarks::MatrixRunner runner;
            const auto benchReport = runner.Benchmark(request, capabilities);
            if (normalizedFormat == "json")
            {
                return CommandOutput{
                    kSuccessExitCode,
                    us4::runtime::benchmarks::MatrixRunner::RenderJson(benchReport, "bench"),
                    {},
                };
            }

            std::ostringstream output;
            output << "execution: bench\n";
            output << "bench.requested_profile: " << benchReport.requestedProfileId << '\n';
            output << "bench.recommended_profile: " << benchReport.recommendedProfileId << '\n';
            output << "bench.selected_profile: " << benchReport.selectedProfileId << '\n';
            output << "bench.selected_backend: " << benchReport.selectedBackend << '\n';
            output << "bench.selected_score: " << benchReport.selectedScore << '\n';
            output << "bench.store_path: " << benchReport.storePath.string() << '\n';
            output << "bench.persisted: " << (benchReport.persisted ? "yes" : "no") << '\n';
            output << "bench.sample_count: " << benchReport.samples.size() << '\n';
            for (std::size_t index = 0; index < benchReport.samples.size(); ++index)
            {
                const auto& sample = benchReport.samples[index];
                const std::size_t sampleNumber = index + 1U;
                output << "bench.sample_" << sampleNumber << ".benchmark: " << sample.benchmarkName
                       << '\n';
                output << "bench.sample_" << sampleNumber << ".backend: " << sample.backend << '\n';
                output << "bench.sample_" << sampleNumber << ".profile: " << sample.profileId
                       << '\n';
                output << "bench.sample_" << sampleNumber
                       << ".supported: " << (sample.supported ? "yes" : "no") << '\n';
                output << "bench.sample_" << sampleNumber << ".score: " << sample.score << '\n';
            }
            output << "bench_status: completed\n";

            return CommandOutput{
                kSuccessExitCode,
                output.str(),
                {},
            };
        }

        const us4::core::RuntimePlan plan =
            us4::core::RuntimeContext::BuildPlan(request, capabilities);

        if (command.kind == CommandKind::kServe)
        {
            if (normalizedFormat == "json")
            {
                return CommandOutput{
                    kNotImplementedExitCode,
                    RenderServeJson(plan),
                    "Serve pipeline scaffolding is ready, but request handling is not "
                    "implemented yet.\n",
                };
            }

            std::ostringstream serveOutput;
            serveOutput << us4::core::FormatRuntimePlan(plan);
            serveOutput << "execution: serve\n";
            serveOutput << "serve.status: scaffold-only\n";
            serveOutput << "serve.allow_npu: " << (plan.request.allowNpu ? "yes" : "no") << '\n';
            serveOutput << "serve.fallback_count: " << plan.fallbacks.size() << '\n';
            AppendIssueCodes(serveOutput, "serve", plan.issues);

            return CommandOutput{
                kNotImplementedExitCode,
                serveOutput.str(),
                "Serve pipeline scaffolding is ready, but request handling is not implemented "
                "yet.\n",
            };
        }

        std::ostringstream output;
        output << us4::core::FormatRuntimePlan(plan);
        output << "prompt_chars: " << command.prompt.size() << '\n';
        if (plan.mode == us4::runtime::backends::RuntimeMode::kCpuOnly ||
            plan.backend.kind == us4::runtime::backends::BackendKind::kCpu)
        {
            const auto runResult =
                us4::core::ExecuteCpuScalarRun(plan, command.prompt, command.modelPath);
            if (!runResult.ok)
            {
                output << "execution: cpu-scalar-failed\n";
                return CommandOutput{
                    kNotImplementedExitCode,
                    normalizedFormat == "json"
                        ? RenderRunPlanJson(plan, command.prompt, "cpu-scalar-failed", "failed",
                                            output.str())
                        : output.str(),
                    runResult.error + '\n',
                };
            }

            output << "execution: cpu-scalar\n";
            output << "model_path: " << runResult.report.modelPath << '\n';
            output << "model_format: "
                   << us4::runtime::adapters::ToString(runResult.report.modelFormat) << '\n';
            output << "prefill_tokens:";
            for (const auto token : runResult.report.prefillTokens)
            {
                output << ' ' << token;
            }
            output << '\n';
            output << "generated_tokens:";
            for (const auto token : runResult.report.generatedTokens)
            {
                output << ' ' << token;
            }
            output << '\n';
            output << "generated_text: " << runResult.report.generatedText << '\n';
            output << "generation.prefill_tokens: " << runResult.report.frame.prefillTokens << '\n';
            output << "generation.decode_tokens: " << runResult.report.frame.decodeTokens << '\n';
            output << "generation.reached_eos: "
                   << (runResult.report.frame.reachedEos ? "yes" : "no") << '\n';
            output << "kernel.scalar_matmul_checksum: " << runResult.report.scalarMatMulChecksum
                   << '\n';
            output << "kernel.scalar_attention_checksum: "
                   << runResult.report.scalarAttentionChecksum << '\n';
            output << "kv.segment_count: " << runResult.report.kvStats.segmentCount << '\n';
            output << "kv.pinned_segment_count: " << runResult.report.kvStats.pinnedSegmentCount
                   << '\n';
            output << "kv.device_hit_rate_pct: "
                   << FormatPercent(runResult.report.kvTierTelemetry.deviceHitRate) << '\n';
            output << "kv.host_hit_rate_pct: "
                   << FormatPercent(runResult.report.kvTierTelemetry.hostHitRate) << '\n';
            output << "kv.storage_hit_rate_pct: "
                   << FormatPercent(runResult.report.kvTierTelemetry.storageHitRate) << '\n';
            output << "kv.summary_hit_rate_pct: "
                   << FormatPercent(runResult.report.kvTierTelemetry.summaryHitRate) << '\n';
            output << "kv.host_bytes: " << runResult.report.kvStats.hostBytes << '\n';
            output << "kv.storage_bytes: " << runResult.report.kvStats.storageBytes << '\n';
            output << "kv.summary_bytes: " << runResult.report.kvStats.summaryBytes << '\n';
            output << "kv.evictions: " << runResult.report.kvStats.evictionCount << '\n';
            output << "prefix_cache.entries: " << runResult.report.prefixCacheEntries << '\n';
            output << "prefix_cache.warm_entries: " << runResult.report.prefixCacheWarmEntries
                   << '\n';
            output << "multimodal_cache.entry_count: "
                   << runResult.report.multimodalCacheTelemetry.entryCount << '\n';
            output << "multimodal_cache.hit_count: "
                   << runResult.report.multimodalCacheTelemetry.hitCount << '\n';
            output << "multimodal_cache.miss_count: "
                   << runResult.report.multimodalCacheTelemetry.missCount << '\n';
            output << "multimodal_cache.resident_bytes: "
                   << runResult.report.multimodalCacheTelemetry.residentBytes << '\n';
            output << "multimodal_cache.image_entries: "
                   << runResult.report.multimodalCacheTelemetry.imageEntries << '\n';
            output << "multimodal_cache.audio_entries: "
                   << runResult.report.multimodalCacheTelemetry.audioEntries << '\n';
            output << "multimodal_cache.video_entries: "
                   << runResult.report.multimodalCacheTelemetry.videoEntries << '\n';
            output << "moe.route_count: " << runResult.report.moeRouteCount << '\n';
            output << "moe.host_route_count: " << runResult.report.moeHostRouteCount << '\n';
            output << "moe.router_entropy: " << runResult.report.moeRouterEntropy << '\n';
            output << "moe.load_balance_loss: " << runResult.report.moeLoadBalanceLoss << '\n';
            output << "moe.hot_experts: " << runResult.report.moeHotExperts << '\n';
            output << "moe.warm_experts: " << runResult.report.moeWarmExperts << '\n';
            output << "moe.cold_experts: " << runResult.report.moeColdExperts << '\n';
            output << "moe.prefetch_observation_count: "
                   << runResult.report.moePrefetchTelemetry.observationCount << '\n';
            output << "moe.prefetch_prediction_count: "
                   << runResult.report.moePrefetchTelemetry.predictionCount << '\n';
            output << "moe.prefetch_hit_count: " << runResult.report.moePrefetchTelemetry.hitCount
                   << '\n';
            output << "moe.prefetch_miss_count: " << runResult.report.moePrefetchTelemetry.missCount
                   << '\n';
            output << "moe.prefetch_hit_ratio_pct: "
                   << (runResult.report.moePrefetchTelemetry.hitRatio * 100.0F) << '\n';
            output << "moe.sparsity_cache_entries: "
                   << runResult.report.moeSparsityTelemetry.entryCount << '\n';
            output << "moe.sparsity_cache_hit_count: "
                   << runResult.report.moeSparsityTelemetry.hitCount << '\n';
            output << "moe.sparsity_cache_miss_count: "
                   << runResult.report.moeSparsityTelemetry.missCount << '\n';
            output << "moe.sparsity_cache_evicted_entries: "
                   << runResult.report.moeSparsityTelemetry.evictedEntries << '\n';
            output << "moe.sparsity_average_pct: "
                   << (runResult.report.moeSparsityTelemetry.averageSparsity * 100.0F) << '\n';
            output << "moe.sparsity_hit_ratio_pct: "
                   << (runResult.report.moeSparsityTelemetry.hitRatio * 100.0F) << '\n';
            output << "telemetry.events: " << runResult.report.telemetryEventCount << '\n';
            output << "speculative.active: "
                   << (runResult.report.speculativeTelemetry.active ? "yes" : "no") << '\n';
            if (runResult.report.speculativeTelemetry.active)
            {
                output << "speculative.decoder: "
                       << runResult.report.speculativeTelemetry.decoder << '\n';
                output << "speculative.draft_model_loaded: "
                       << (runResult.report.speculativeTelemetry.draftModelLoaded ? "yes" : "no")
                       << '\n';
                output << "speculative.acceptance_rate_pct: "
                       << (runResult.report.speculativeTelemetry.acceptanceRate * 100.0F) << '\n';
                output << "speculative.last_step_delta_pct: "
                       << (runResult.report.speculativeTelemetry.lastStepDelta * 100.0F) << '\n';
                output << "speculative.accepted_tokens: "
                       << runResult.report.speculativeTelemetry.acceptedTokens << '\n';
                output << "speculative.rejected_tokens: "
                       << runResult.report.speculativeTelemetry.rejectedTokens << '\n';
                output << "speculative.estimated_speedup: "
                       << runResult.report.speculativeTelemetry.estimatedSpeedup << '\n';
                output << "speculative.step_count: "
                       << runResult.report.speculativeTelemetry.steps.size() << '\n';
                for (std::size_t index = 0;
                     index < runResult.report.speculativeTelemetry.steps.size(); ++index)
                {
                    const auto& step = runResult.report.speculativeTelemetry.steps[index];
                    const std::size_t stepNumber = index + 1U;
                    output << "speculative.step_" << stepNumber
                           << ".acceptance_rate_pct: " << (step.acceptanceRate * 100.0F) << '\n';
                    output << "speculative.step_" << stepNumber
                           << ".delta_pct: " << (step.deltaFromPreviousStep * 100.0F) << '\n';
                }
            }
            output << "run_status: completed\n";
            return CommandOutput{
                kSuccessExitCode,
                normalizedFormat == "json"
                    ? RenderRunCpuJson(plan, command.prompt, runResult, output.str())
                    : output.str(),
                {},
            };
        }

        std::string_view executionKind = "scaffold-only";
        switch (plan.backend.kind)
        {
        case us4::runtime::backends::BackendKind::kCuda:
            executionKind = "cuda-dry-run";
            AppendCudaDryRun(output, plan, capabilities, command.prompt, command.modelPath);
            break;
        case us4::runtime::backends::BackendKind::kDirectML:
            executionKind = "directml-dry-run";
            AppendDirectMlDryRun(output, plan, capabilities, command.prompt, command.modelPath);
            break;
        case us4::runtime::backends::BackendKind::kVulkan:
            executionKind = "vulkan-dry-run";
            AppendVulkanDryRun(output, plan, capabilities);
            break;
        case us4::runtime::backends::BackendKind::kWindowsMl:
            executionKind = "windows-ml-dry-run";
            AppendWindowsMlDryRun(output, plan, capabilities);
            break;
        case us4::runtime::backends::BackendKind::kCpu:
            executionKind = "scaffold-only";
            output << "execution: scaffold-only\n";
            break;
        }

        return CommandOutput{
            kNotImplementedExitCode,
            normalizedFormat == "json"
                ? RenderRunPlanJson(plan, command.prompt, executionKind, "dry-run", output.str())
                : output.str(),
            "Run pipeline scaffolding is ready, but model execution is not implemented yet.\n",
        };
    }

} // namespace us4::cli
