#include "us4/profiles/profile_catalog.h"
#include "us4/runtime/backends/vulkan/vulkan_context.h"
#include "us4/runtime/backends/vulkan/vulkan_execution_plan.h"
#include "us4/runtime/backends/windows_ml/layer_offloader.h"
#include "us4/runtime/backends/windows_ml/mixed_dispatch_planner.h"
#include "us4/runtime/backends/windows_ml/power_thermal_monitor.h"
#include "us4/runtime/backends/windows_ml/windows_ml_execution_plan.h"
#include "us4/runtime/backends/windows_ml/winml_adapter.h"
#include "us4/runtime/benchmarks/benchmark_registry.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace
{

    using Clock = std::chrono::steady_clock;

    struct TimingBreakdown
    {
        double planMs = 0.0;
        double initializeMs = 0.0;
        double phaseMs = 0.0;
        double totalMs = 0.0;
    };

    struct GateResult
    {
        std::string name;
        std::string backend;
        std::string status;
        TimingBreakdown timing;
        std::vector<std::string> details;
        std::string jsonFingerprint;
    };

    template <typename Fn> double Measure(Fn&& fn)
    {
        const auto startedAt = Clock::now();
        fn();
        const auto finishedAt = Clock::now();
        return std::chrono::duration<double, std::milli>(finishedAt - startedAt).count();
    }

    std::string EscapeJson(std::string_view value)
    {
        std::string escaped;
        escaped.reserve(value.size());
        for (const char ch : value)
        {
            switch (ch)
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
            default:
                escaped.push_back(ch);
                break;
            }
        }
        return escaped;
    }

    us4::runtime::backends::RuntimeMode ParseRuntimeMode(const std::string& mode)
    {
        using us4::runtime::backends::RuntimeMode;

        if (mode == "FULL")
        {
            return RuntimeMode::kFull;
        }
        if (mode == "BALANCED")
        {
            return RuntimeMode::kBalanced;
        }
        if (mode == "DEGRADED")
        {
            return RuntimeMode::kDegraded;
        }
        if (mode == "ULTRA_LOW")
        {
            return RuntimeMode::kUltraLow;
        }
        if (mode == "MICRO")
        {
            return RuntimeMode::kMicro;
        }
        if (mode == "NANO")
        {
            return RuntimeMode::kNano;
        }
        return RuntimeMode::kCpuOnly;
    }

    us4::profiles::RuntimeProfile
    ResolveProfile(const us4::runtime::benchmarks::BenchmarkCase& benchmark)
    {
        if (const auto profile = us4::profiles::ProfileCatalog::FindById(benchmark.profileId);
            profile.has_value())
        {
            return *profile;
        }

        return us4::profiles::RuntimeProfile{
            .id = benchmark.profileId,
            .displayName = benchmark.profileId,
            .mode = ParseRuntimeMode(benchmark.runtimeMode),
            .preferredBackend = benchmark.backend,
            .targetContextTokens = 4096U,
            .targetBatchSize = benchmark.backend == "vulkan" ? 2U : 1U,
        };
    }

    bool SupportsSpeculative(std::string_view modelId)
    {
        return modelId.find("qwen") != std::string_view::npos ||
               modelId.find("llama") != std::string_view::npos;
    }

    us4::runtime::backends::HardwareCapabilities MakeDiscreteGpuCapabilities()
    {
        using namespace us4::runtime::backends;

        HardwareCapabilities capabilities{};
        capabilities.hasVulkan = true;
        capabilities.hasAvx2 = true;
        capabilities.discreteGpuCount = 1;
        capabilities.primaryAdapterName = "Radeon RX Test";
        capabilities.primaryAdapterVendor = BackendVendor::kAmd;
        capabilities.primaryAdapterClass = DeviceClass::kDiscreteGpu;
        capabilities.budget.deviceBytes = 12ULL * 1024ULL * 1024ULL * 1024ULL;
        capabilities.budget.hostBytes = 32ULL * 1024ULL * 1024ULL * 1024ULL;
        capabilities.budget.storageBytes = 256ULL * 1024ULL * 1024ULL * 1024ULL;
        return capabilities;
    }

    us4::runtime::backends::HardwareCapabilities MakeHybridCapabilities()
    {
        auto capabilities = MakeDiscreteGpuCapabilities();
        capabilities.hasNpu = true;
        capabilities.npuCount = 1;
        capabilities.npuName = "Ryzen AI Test";
        capabilities.npuVendor = us4::runtime::backends::BackendVendor::kMicrosoft;
        return capabilities;
    }

    us4::runtime::backends::BackendDescriptor
    MakeVulkanBackend(const us4::runtime::backends::HardwareCapabilities& capabilities)
    {
        using namespace us4::runtime::backends;

        return BackendDescriptor{
            .kind = BackendKind::kVulkan,
            .name = "vulkan",
            .displayName = "Vulkan Compute",
            .deviceClass = capabilities.primaryAdapterClass,
            .vendor = capabilities.primaryAdapterVendor,
            .availability = BackendAvailability::kAvailable,
            .defaultPrecision = PrecisionMode::kFp16,
            .selectionRank = 10U,
            .maxContextTokensHint = 8192U,
            .supportsPagedKv = true,
            .supportsSpeculative = true,
            .supportsUnifiedMemory = capabilities.hasUnifiedMemory,
            .budget = capabilities.budget,
        };
    }

    us4::runtime::backends::BackendDescriptor
    MakeWindowsMlBackend(const us4::runtime::backends::HardwareCapabilities& capabilities)
    {
        using namespace us4::runtime::backends;

        return BackendDescriptor{
            .kind = BackendKind::kWindowsMl,
            .name = "windows-ml",
            .displayName = "Windows ML",
            .deviceClass = DeviceClass::kNpu,
            .vendor = capabilities.npuVendor,
            .availability = BackendAvailability::kOptIn,
            .defaultPrecision = PrecisionMode::kInt8,
            .selectionRank = 20U,
            .maxContextTokensHint = 4096U,
            .supportsPagedKv = false,
            .supportsSpeculative = false,
            .supportsNpuOffload = true,
            .requiresOptIn = true,
            .budget = capabilities.budget,
        };
    }

    us4::runtime::backends::SessionRequest
    MakeRequest(const us4::runtime::benchmarks::BenchmarkCase& benchmark,
                const us4::profiles::RuntimeProfile& profile, const bool allowNpu)
    {
        using namespace us4::runtime::backends;

        return SessionRequest{
            .modelId = benchmark.modelId,
            .preferredBackend = benchmark.backend,
            .mode = ParseRuntimeMode(benchmark.runtimeMode),
            .precision =
                benchmark.backend == "windows-ml" ? PrecisionMode::kInt8 : PrecisionMode::kFp16,
            .maxContextTokens = static_cast<std::uint32_t>(profile.targetContextTokens),
            .maxGenerationTokens = static_cast<std::uint32_t>(benchmark.generationTokens),
            .enableSpeculative = SupportsSpeculative(benchmark.modelId),
            .allowNpu = allowNpu,
            .preferLowLatency = benchmark.backend == "windows-ml",
            .preferMaxThroughput = benchmark.backend == "vulkan",
        };
    }

    us4::runtime::adapters::RuntimeBinding
    MakeBinding(const us4::runtime::backends::BackendDescriptor& backend,
                const us4::profiles::RuntimeProfile& profile, const std::string& modelId,
                const bool allowNpu)
    {
        return us4::runtime::adapters::RuntimeBinding{
            .backend = backend,
            .profileId = profile.id,
            .mode = profile.mode,
            .modelId = modelId,
            .allowNpu = allowNpu,
        };
    }

    std::vector<us4::runtime::backends::windows_ml::LayerDescriptor> SampleLayers()
    {
        using namespace us4::runtime::backends::windows_ml;

        return {
            {
                .name = "embed",
                .kind = LayerKind::kEmbedding,
            },
            {
                .name = "prefill.ffn",
                .kind = LayerKind::kDensePrefill,
            },
            {
                .name = "decode.ffn",
                .kind = LayerKind::kDenseDecode,
                .latencySensitive = true,
            },
            {
                .name = "attention",
                .kind = LayerKind::kAttention,
            },
            {
                .name = "kv-compress",
                .kind = LayerKind::kKvCompression,
            },
        };
    }

    us4::runtime::backends::windows_ml::PowerThermalSnapshot
    MakePowerSnapshot(const std::string& benchmarkName)
    {
        using namespace us4::runtime::backends::windows_ml;

        if (benchmarkName == "windows_ml_qwen_thermal_throttle")
        {
            return PowerThermalSnapshot{
                .powerSource = PowerSource::kBattery,
                .thermalState = ThermalState::kThrottled,
                .batteryPercent = 18U,
                .batterySaverActive = true,
                .etwThrottleActive = true,
                .usingSyntheticTelemetry = true,
            };
        }

        return PowerThermalSnapshot{
            .powerSource = PowerSource::kAc,
            .thermalState = ThermalState::kNominal,
            .batteryPercent = 100U,
            .batterySaverActive = false,
            .etwThrottleActive = false,
            .usingSyntheticTelemetry = true,
        };
    }

    std::string RenderArray(const std::vector<std::string>& values)
    {
        std::ostringstream json;
        json << '[';
        for (std::size_t index = 0; index < values.size(); ++index)
        {
            if (index > 0U)
            {
                json << ',';
            }
            json << '"' << EscapeJson(values[index]) << '"';
        }
        json << ']';
        return json.str();
    }

    GateResult RunVulkanCase(const us4::runtime::benchmarks::BenchmarkCase& benchmark)
    {
        using namespace us4::runtime::backends;
        using namespace us4::runtime::backends::vulkan;

        const auto profile = ResolveProfile(benchmark);
        const auto capabilities = MakeDiscreteGpuCapabilities();
        const auto backend = MakeVulkanBackend(capabilities);
        const auto request = MakeRequest(benchmark, profile, false);
        const auto binding = MakeBinding(backend, profile, benchmark.modelId, false);

        TimingBreakdown timing{};
        VulkanExecutionPlan plan{};
        timing.planMs = Measure(
            [&]()
            {
                plan = BuildVulkanExecutionPlan({
                    .binding = binding,
                    .request = request,
                    .adapterId = benchmark.adapterId,
                    .targetBatchSize = profile.targetBatchSize,
                    .modelSupportsMoE = false,
                    .modelSupportsSpeculative = SupportsSpeculative(benchmark.modelId),
                });
            });

        VulkanContext context({
            .preferIntegratedGpu = false,
            .enableValidationLayer = false,
            .allowDescriptorBuffer = true,
            .preferAsyncTransfers = true,
        });
        bool initialized = false;
        bool bound = false;
        timing.initializeMs = Measure([&]() { initialized = context.Initialize(capabilities); });
        timing.phaseMs = Measure(
            [&]() { bound = initialized && context.BindExecutionPlan(benchmark.modelId, plan); });
        timing.totalMs = timing.planMs + timing.initializeMs + timing.phaseMs;

        std::vector<std::string> stageSequence;
        stageSequence.reserve(plan.steps.size());
        for (const auto& step : plan.steps)
        {
            stageSequence.emplace_back(ToString(step.stage));
        }

        std::vector<std::string> issues;
        for (const auto& issue : plan.issues)
        {
            issues.push_back(issue.code);
        }

        std::vector<std::string> failures;
        if (!initialized)
        {
            failures.push_back("context_initialize_failed");
        }
        if (!bound)
        {
            failures.push_back("context_bind_failed");
        }
        if (context.State() != VulkanContextState::kBound)
        {
            failures.push_back("context_not_bound");
        }
        if (plan.steps.size() < 4U)
        {
            failures.push_back("step_count_below_expected");
        }
        if (context.Stats().loadedKernelCount < 3U)
        {
            failures.push_back("loaded_kernel_count_below_expected");
        }
        if (context.Stats().requiredKernelCount != 3U)
        {
            failures.push_back("required_kernel_count_mismatch");
        }
        if (!issues.empty())
        {
            failures.push_back("unexpected_plan_issues");
        }
        if (context.DescriptorArena().setCount < 1U)
        {
            failures.push_back("descriptor_sets_missing");
        }
        if (plan.steps.empty() || plan.steps.back().stage != VulkanKernelStage::kDecode)
        {
            failures.push_back("decode_terminal_stage_missing");
        }

        std::ostringstream fingerprint;
        fingerprint << '{' << "\"context_state\":\"" << EscapeJson(ToString(context.State()))
                    << "\","
                    << "\"step_count\":" << plan.steps.size() << ','
                    << "\"stage_sequence\":" << RenderArray(stageSequence) << ','
                    << "\"queue_class\":" << static_cast<int>(context.Queue().queueClass) << ','
                    << "\"loaded_kernel_count\":" << context.Stats().loadedKernelCount << ','
                    << "\"required_kernel_count\":" << context.Stats().requiredKernelCount << ','
                    << "\"descriptor_set_count\":" << context.DescriptorArena().setCount << ','
                    << "\"issue_codes\":" << RenderArray(issues) << '}';

        return GateResult{
            .name = benchmark.name,
            .backend = benchmark.backend,
            .status = failures.empty() ? "pass" : "fail",
            .timing = timing,
            .details = std::move(failures),
            .jsonFingerprint = fingerprint.str(),
        };
    }

    GateResult RunWindowsMlCase(const us4::runtime::benchmarks::BenchmarkCase& benchmark)
    {
        using namespace us4::runtime::backends;
        using namespace us4::runtime::backends::vulkan;
        using namespace us4::runtime::backends::windows_ml;

        const auto profile = ResolveProfile(benchmark);
        const bool noNpuFallbackCase = benchmark.name == "windows_ml_qwen_no_npu_fallback";
        const auto capabilities =
            noNpuFallbackCase ? MakeDiscreteGpuCapabilities() : MakeHybridCapabilities();
        const auto backend = MakeWindowsMlBackend(capabilities);
        const auto request = MakeRequest(benchmark, profile, true);
        const auto binding = MakeBinding(backend, profile, benchmark.modelId, true);

        TimingBreakdown timing{};
        WindowsMlExecutionPlan plan{};
        timing.planMs = Measure(
            [&]()
            {
                plan = BuildWindowsMlExecutionPlan({
                    .binding = binding,
                    .request = request,
                    .adapterId = benchmark.adapterId,
                    .targetBatchSize = profile.targetBatchSize,
                    .modelSupportsMoE = false,
                    .modelSupportsSpeculative = SupportsSpeculative(benchmark.modelId),
                    .modelSupportsVision = false,
                });
            });

        WinMlAdapter adapter({
            .allowCpuFallback = true,
            .preferStaticShapes = true,
            .enableTelemetry = true,
        });
        bool initialized = false;
        bool compiled = false;
        timing.initializeMs = Measure([&]() { initialized = adapter.Initialize(capabilities); });
        timing.phaseMs = Measure(
            [&]() { compiled = initialized && adapter.CompileGraph(benchmark.modelId, plan); });
        const auto artifact = adapter.SessionArtifact();

        const auto layers = SampleLayers();
        const auto powerSnapshot = MakePowerSnapshot(benchmark.name);
        const auto powerPolicy = PowerThermalMonitor::SelectPolicy(powerSnapshot);
        const auto powerIssues = PowerThermalMonitor::BuildIssues(powerSnapshot);
        const auto dispatchTable = LayerOffloader::BuildDispatchTable(plan, layers);
        const auto gpuBinding =
            MakeBinding(MakeVulkanBackend(capabilities), profile, benchmark.modelId, false);
        const auto gpuPlan = BuildVulkanExecutionPlan({
            .binding = gpuBinding,
            .request = request,
            .adapterId = benchmark.adapterId,
            .targetBatchSize = profile.targetBatchSize,
            .modelSupportsMoE = false,
            .modelSupportsSpeculative = SupportsSpeculative(benchmark.modelId),
        });
        const auto mixedPlan = MixedDispatchPlanner::Build(gpuPlan, plan, layers, powerSnapshot,
                                                           artifact ? &*artifact : nullptr);
        timing.totalMs = timing.planMs + timing.initializeMs + timing.phaseMs;

        std::vector<std::string> dispatchTargets;
        dispatchTargets.reserve(dispatchTable.size());
        for (const auto& decision : dispatchTable)
        {
            dispatchTargets.emplace_back(ToString(decision.target));
        }

        std::vector<std::string> mixedTargets;
        mixedTargets.reserve(mixedPlan.slices.size());
        for (const auto& slice : mixedPlan.slices)
        {
            mixedTargets.emplace_back(ToString(slice.target));
        }

        std::vector<std::string> issueCodes;
        for (const auto& issue : plan.issues)
        {
            issueCodes.push_back(issue.code);
        }

        std::vector<std::string> powerIssueCodes;
        for (const auto& issue : powerIssues)
        {
            powerIssueCodes.push_back(issue.code);
        }

        std::vector<std::string> failures;
        if (!initialized)
        {
            failures.push_back("adapter_initialize_failed");
        }
        if (!compiled)
        {
            failures.push_back("graph_compile_failed");
        }
        if (!plan.optInSatisfied)
        {
            failures.push_back("opt_in_not_satisfied");
        }
        if (plan.partitions.size() != 5U)
        {
            failures.push_back("partition_count_mismatch");
        }
        if (dispatchTable.size() != 5U)
        {
            failures.push_back("dispatch_table_size_mismatch");
        }
        if (mixedPlan.slices.size() != 5U)
        {
            failures.push_back("mixed_dispatch_slice_count_mismatch");
        }
        if (adapter.Stats().npuPartitionCount != 3U)
        {
            if (!noNpuFallbackCase)
            {
                failures.push_back("npu_partition_count_mismatch");
            }
        }
        if (adapter.Stats().hostPartitionCount != 2U)
        {
            if (!noNpuFallbackCase)
            {
                failures.push_back("host_partition_count_mismatch");
            }
        }

        if (benchmark.name == "windows_ml_qwen_opt_in")
        {
            if (powerPolicy != PowerDispatchPolicy::kNominal)
            {
                failures.push_back("unexpected_power_policy");
            }
            if (!mixedPlan.npuDenseActive)
            {
                failures.push_back("npu_dense_inactive");
            }
            if (mixedPlan.degradedByPolicy)
            {
                failures.push_back("unexpected_policy_degradation");
            }
            if (mixedPlan.npuDemotionCount != 0U)
            {
                failures.push_back("unexpected_npu_demotions");
            }
        }

        if (benchmark.name == "windows_ml_qwen_thermal_throttle")
        {
            if (powerPolicy != PowerDispatchPolicy::kThermalThrottle)
            {
                failures.push_back("thermal_policy_not_selected");
            }
            if (mixedPlan.npuDenseActive)
            {
                failures.push_back("npu_dense_not_demoted");
            }
            if (!mixedPlan.degradedByPolicy)
            {
                failures.push_back("policy_degradation_missing");
            }
            if (mixedPlan.npuDemotionCount != 3U)
            {
                failures.push_back("npu_demotion_count_mismatch");
            }
            if (powerIssues.size() != 2U)
            {
                failures.push_back("power_issue_count_mismatch");
            }
        }

        if (benchmark.name == "windows_ml_qwen_no_npu_fallback")
        {
            if (!artifact.has_value())
            {
                failures.push_back("session_artifact_missing");
            }
            else
            {
                if (artifact->compileTarget != WinMlCompileTarget::kCpuFallback)
                {
                    failures.push_back("compile_target_not_cpu_fallback");
                }
                if (artifact->fallbackReason != "npu-unavailable")
                {
                    failures.push_back("fallback_reason_mismatch");
                }
                if (!artifact->cpuFallbackArmed)
                {
                    failures.push_back("cpu_fallback_not_armed");
                }
            }
            if (adapter.Stats().npuPartitionCount != 0U)
            {
                failures.push_back("npu_partition_count_not_zero");
            }
            if (adapter.Stats().hostPartitionCount != 5U)
            {
                failures.push_back("host_partition_count_not_rewritten");
            }
            if (adapter.Stats().cpuFallbackPartitionCount == 0U)
            {
                failures.push_back("cpu_fallback_partition_missing");
            }
            if (mixedPlan.npuDenseActive)
            {
                failures.push_back("npu_dense_should_be_inactive");
            }
            if (!mixedPlan.cpuFallbackPresent)
            {
                failures.push_back("cpu_fallback_target_missing");
            }
            if (mixedPlan.degradedByPolicy)
            {
                failures.push_back("unexpected_policy_degradation");
            }
            if (powerPolicy != PowerDispatchPolicy::kNominal)
            {
                failures.push_back("unexpected_power_policy");
            }
        }

        std::ostringstream fingerprint;
        fingerprint << '{' << "\"adapter_state\":\"" << EscapeJson(ToString(adapter.State()))
                    << "\","
                    << "\"compile_target\":\""
                    << EscapeJson(artifact.has_value() ? ToString(artifact->compileTarget) : "")
                    << "\","
                    << "\"fallback_reason\":\""
                    << EscapeJson(artifact.has_value() ? artifact->fallbackReason : "") << "\","
                    << "\"cpu_fallback_armed\":"
                    << (artifact.has_value() && artifact->cpuFallbackArmed ? "true" : "false")
                    << ',' << "\"partition_count\":" << plan.partitions.size() << ','
                    << "\"npu_partition_count\":" << adapter.Stats().npuPartitionCount << ','
                    << "\"host_partition_count\":" << adapter.Stats().hostPartitionCount << ','
                    << "\"cpu_fallback_partition_count\":"
                    << adapter.Stats().cpuFallbackPartitionCount << ','
                    << "\"dispatch_targets\":" << RenderArray(dispatchTargets) << ','
                    << "\"mixed_dispatch_targets\":" << RenderArray(mixedTargets) << ','
                    << "\"mixed_dispatch_active\":" << (capabilities.hasVulkan ? "true" : "false")
                    << ',' << "\"mixed_dispatch_degraded\":"
                    << (mixedPlan.degradedByPolicy ? "true" : "false") << ','
                    << "\"mixed_dispatch_npu_demotions\":" << mixedPlan.npuDemotionCount << ','
                    << "\"policy\":\"" << EscapeJson(ToString(powerPolicy)) << "\","
                    << "\"issue_codes\":" << RenderArray(issueCodes) << ','
                    << "\"power_issue_codes\":" << RenderArray(powerIssueCodes) << '}';

        return GateResult{
            .name = benchmark.name,
            .backend = benchmark.backend,
            .status = failures.empty() ? "pass" : "fail",
            .timing = timing,
            .details = std::move(failures),
            .jsonFingerprint = fingerprint.str(),
        };
    }

    std::string RenderReport(const std::vector<GateResult>& results)
    {
        std::ostringstream report;
        report << "{\n";
        report << "  \"suite\": \"hybrid-planner-gate\",\n";
        report << "  \"cases\": [\n";
        for (std::size_t index = 0; index < results.size(); ++index)
        {
            const auto& result = results[index];
            report << "    {\n";
            report << "      \"name\": \"" << EscapeJson(result.name) << "\",\n";
            report << "      \"backend\": \"" << EscapeJson(result.backend) << "\",\n";
            report << "      \"status\": \"" << result.status << "\",\n";
            report << "      \"timing_ms\": {\n";
            report << "        \"plan\": " << std::fixed << std::setprecision(3)
                   << result.timing.planMs << ",\n";
            report << "        \"initialize\": " << result.timing.initializeMs << ",\n";
            report << "        \"phase\": " << result.timing.phaseMs << ",\n";
            report << "        \"total\": " << result.timing.totalMs << "\n";
            report << "      },\n";
            report << "      \"details\": " << RenderArray(result.details) << ",\n";
            report << "      \"fingerprint\": " << result.jsonFingerprint << '\n';
            report << "    }";
            if (index + 1U < results.size())
            {
                report << ',';
            }
            report << '\n';
        }
        report << "  ]\n";
        report << "}\n";
        return report.str();
    }

} // namespace

int main()
{
    using us4::runtime::benchmarks::BenchmarkRegistry;

    std::vector<GateResult> results;

    for (const auto& benchmark : BenchmarkRegistry::CasesForBackend("vulkan"))
    {
        if (benchmark.backend == "vulkan" && benchmark.participatesInCorrectnessGate)
        {
            results.push_back(RunVulkanCase(benchmark));
        }
    }

    for (const auto& benchmark : BenchmarkRegistry::CasesForBackend("windows-ml"))
    {
        if (benchmark.backend == "windows-ml" && benchmark.participatesInCorrectnessGate)
        {
            results.push_back(RunWindowsMlCase(benchmark));
        }
    }

    bool failed = false;
    for (const auto& result : results)
    {
        failed = failed || result.status != "pass";
        std::cout << result.name << " backend=" << result.backend << " status=" << result.status
                  << " plan_ms=" << std::fixed << std::setprecision(3) << result.timing.planMs
                  << " initialize_ms=" << result.timing.initializeMs
                  << " phase_ms=" << result.timing.phaseMs << " total_ms=" << result.timing.totalMs;
        if (!result.details.empty())
        {
            std::cout << " details=";
            for (std::size_t index = 0; index < result.details.size(); ++index)
            {
                if (index > 0U)
                {
                    std::cout << ',';
                }
                std::cout << result.details[index];
            }
        }
        std::cout << '\n';
    }

    const std::filesystem::path reportDirectory =
        std::filesystem::path("runtime") / "benchmarks" / "correctness" / "reports";
    std::filesystem::create_directories(reportDirectory);
    const std::filesystem::path reportPath = reportDirectory / "hybrid_planner_gate.json";
    std::ofstream report(reportPath, std::ios::binary);
    report << RenderReport(results);

    if (failed)
    {
        std::cerr << "Hybrid planner gate failed. Report: " << reportPath.string() << '\n';
        return 1;
    }

    std::cout << "Hybrid planner gate passed. Report: " << reportPath.string() << '\n';
    return 0;
}
