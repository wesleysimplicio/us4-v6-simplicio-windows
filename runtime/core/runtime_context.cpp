#include "runtime/core/runtime_context.h"

#include "runtime/core/runtime_mode.h"
#include "us4/profiles/profile_catalog.h"
#include "us4/runtime/adapters/i_us4_windows_adapter.h"
#include "us4/runtime/backends/backend_selector.h"
#include "us4/runtime/tuning/profile_store.h"

#include <algorithm>
#include <sstream>

namespace us4::core
{
    namespace
    {
        struct ProfileSelection
        {
            std::string profileId;
            bool fromStore = false;
        };

        ProfileSelection
        ResolveProfileSelection(const ModelDescriptor& model,
                                us4::runtime::backends::RuntimeMode requestedMode,
                                us4::runtime::backends::RuntimeMode selectedMode,
                                const us4::runtime::backends::HardwareCapabilities& capabilities)
        {
            using us4::runtime::backends::RuntimeMode;

            if (requestedMode == RuntimeMode::kCpuOnly)
            {
                return {"cpu-only", false};
            }
            if (requestedMode == RuntimeMode::kFull)
            {
                return {"full", false};
            }
            if (requestedMode == RuntimeMode::kDegraded)
            {
                return {"degraded", false};
            }
            if (requestedMode == RuntimeMode::kUltraLow)
            {
                return {"ultra-low", false};
            }
            if (requestedMode == RuntimeMode::kMicro)
            {
                return {"micro", false};
            }
            if (requestedMode == RuntimeMode::kNano)
            {
                return {"nano", false};
            }

            us4::runtime::tuning::ProfileStore profileStore;
            if (const auto storedProfileId = profileStore.LoadProfileId(capabilities);
                storedProfileId.has_value())
            {
                return {*storedProfileId, true};
            }
            if (!model.defaultProfileId.empty())
            {
                const auto defaultProfile =
                    us4::profiles::ProfileCatalog::FindById(model.defaultProfileId);
                if (!defaultProfile.has_value() || requestedMode != RuntimeMode::kBalanced ||
                    defaultProfile->mode == selectedMode)
                {
                    return {model.defaultProfileId, false};
                }
            }
            return {us4::profiles::ProfileCatalog::RecommendId(capabilities), false};
        }

        bool MatchesPreferredBackend(const std::string& preferredBackend,
                                     const us4::runtime::backends::BackendDescriptor& backend)
        {
            if (preferredBackend.empty() || preferredBackend == "auto")
            {
                return true;
            }

            return preferredBackend == backend.name;
        }

        us4::runtime::adapters::ModelResidencyPlan
        BuildResidencyFallback(const us4::runtime::backends::SessionRequest& request,
                               const us4::runtime::adapters::RuntimeBinding& binding,
                               const us4::profiles::RuntimeProfile& profile)
        {
            return us4::runtime::adapters::ModelResidencyPlan{
                .modelId = request.modelId,
                .backendName = binding.backend.name,
                .profileId = profile.id,
                .expectedHostBytes = static_cast<std::size_t>(request.maxContextTokens) * 4096ULL,
                .expectedDeviceBytes = static_cast<std::size_t>(request.maxContextTokens) * 2048ULL,
                .enableKvTiering = profile.enableKvTiering,
                .enableSpeculative = profile.enableSpeculative && request.enableSpeculative,
            };
        }
    } // namespace

    RuntimePlan
    RuntimeContext::BuildPlan(const us4::runtime::backends::SessionRequest& request,
                              const us4::runtime::backends::HardwareCapabilities& capabilities)
    {
        using namespace us4::runtime::backends;

        RuntimePlan plan{};
        plan.request = request;

        const auto resolvedModel = ModelRegistry::Resolve(request.modelId);
        if (resolvedModel.has_value())
        {
            plan.model = *resolvedModel;
        }
        else
        {
            plan.model.id = request.modelId;
            plan.model.adapterId = "null";
            plan.model.defaultProfileId = "balanced";
            plan.issues.push_back({"unknown-model", "Model family is not registered yet.", true});
        }

        plan.fallbacks = BackendSelector::SelectFallbacks(request, capabilities);
        if (!plan.fallbacks.empty())
        {
            plan.backend = plan.fallbacks.front();
        }
        else
        {
            plan.backend.kind = BackendKind::kCpu;
            plan.backend.name = "cpu-avx2";
            plan.backend.displayName = "CPU AVX2 Fallback";
            plan.backend.deviceClass = DeviceClass::kCpuOnly;
            plan.backend.vendor = BackendVendor::kMicrosoft;
            plan.backend.defaultPrecision = PrecisionMode::kFp32;
            plan.backend.supportsPagedKv = true;
            plan.backend.supportsMoE = true;
            plan.backend.budget = capabilities.budget;
            plan.issues.push_back(
                {"fallback-backend", "No primary accelerator matched; using CPU fallback.", true});
            plan.fallbacks.push_back(plan.backend);
        }

        plan.mode = request.mode == RuntimeMode::kBalanced
                        ? SelectRuntimeMode(plan.backend, capabilities, request)
                        : request.mode;

        const auto profileSelection =
            ResolveProfileSelection(plan.model, request.mode, plan.mode, capabilities);
        const std::string profileId = profileSelection.profileId;

        const auto profile = us4::profiles::ProfileCatalog::FindById(profileId);
        if (profile.has_value())
        {
            plan.profile = *profile;
            if (profileSelection.fromStore && request.mode == RuntimeMode::kBalanced)
            {
                plan.mode = plan.profile.mode;
            }
        }
        else
        {
            plan.profile.id = profileId;
            plan.profile.displayName = "Unknown";
            plan.profile.mode = plan.mode;
            plan.issues.push_back({"unknown-profile", "Profile id is not registered yet.", true});
        }

        if (profileSelection.fromStore)
        {
            plan.issues.push_back({"profile-store-hit",
                                   "Runtime profile was restored from the persisted hardware "
                                   "fingerprint cache.",
                                   true});
        }

        if (plan.profile.mode != plan.mode)
        {
            plan.issues.push_back(
                {"profile-mode-mismatch",
                 "Resolved profile mode does not match the runtime mode selected for this host.",
                 true});
        }

        if (!MatchesPreferredBackend(plan.profile.preferredBackend, plan.backend))
        {
            plan.issues.push_back(
                {"profile-backend-mismatch",
                 "Profile backend preference differs from the backend selected for this host.",
                 true});
        }

        if (plan.profile.enableSpeculative && !plan.backend.supportsSpeculative)
        {
            plan.issues.push_back(
                {"speculative-unsupported",
                 "Profile enables speculative decoding but the selected backend cannot serve it.",
                 true});
        }

        if (plan.profile.enableMoE && !plan.backend.supportsMoE)
        {
            plan.issues.push_back({"moe-unsupported",
                                   "Profile enables MoE but the selected backend does not expose "
                                   "that capability yet.",
                                   true});
        }

        if (plan.backend.kind == BackendKind::kWindowsMl && !request.allowNpu)
        {
            plan.issues.push_back(
                {"npu-opt-in", "Windows ML/NPU paths require explicit opt-in.", true});
        }

        plan.binding = us4::runtime::adapters::RuntimeBinding{
            .backend = plan.backend,
            .profileId = plan.profile.id,
            .mode = plan.mode,
            .modelId = plan.model.id,
            .allowNpu = request.allowNpu,
        };

        auto adapter = us4::runtime::adapters::CreateNullAdapter();
        if (adapter && adapter->Attach(plan.binding))
        {
            plan.residency = adapter->BuildResidencyPlan(request);
            if (plan.profile.enableKvTiering)
            {
                plan.residency.enableKvTiering = true;
            }
            plan.residency.enableSpeculative =
                plan.profile.enableSpeculative && request.enableSpeculative;
        }
        else
        {
            plan.residency = BuildResidencyFallback(request, plan.binding, plan.profile);
            plan.issues.push_back({"adapter-binding-failed",
                                   "Could not attach the runtime plan to the scaffold adapter.",
                                   true});
        }

        return plan;
    }

    std::string FormatRuntimePlan(const RuntimePlan& plan)
    {
        std::ostringstream output;
        output << "US4 Runtime Plan\n";
        output << "model: " << plan.model.id << '\n';
        output << "family: " << ToString(plan.model.family) << '\n';
        output << "adapter: " << plan.model.adapterId << '\n';
        output << "backend: " << plan.backend.name << '\n';
        output << "mode: " << ToString(plan.mode) << '\n';
        output << "profile: " << plan.profile.id << '\n';
        output << "context_tokens: " << plan.profile.targetContextTokens << '\n';
        output << "batch_size: " << plan.profile.targetBatchSize << '\n';
        output << "residency.backend: " << plan.residency.backendName << '\n';
        output << "residency.profile: " << plan.residency.profileId << '\n';
        output << "residency.host_bytes: " << plan.residency.expectedHostBytes << '\n';
        output << "residency.device_bytes: " << plan.residency.expectedDeviceBytes << '\n';
        output << "residency.kv_tiering: " << (plan.residency.enableKvTiering ? "yes" : "no")
               << '\n';
        output << "residency.speculative: " << (plan.residency.enableSpeculative ? "yes" : "no")
               << '\n';
        output << "supports_moe: " << (plan.model.supportsMoE ? "yes" : "no") << '\n';
        output << "supports_speculative: " << (plan.model.supportsSpeculative ? "yes" : "no")
               << '\n';
        if (!plan.fallbacks.empty())
        {
            output << "fallbacks:\n";
            for (const auto& fallback : plan.fallbacks)
            {
                output << "  - " << fallback.name << '\n';
            }
        }
        if (!plan.issues.empty())
        {
            output << "issues:\n";
            for (const auto& issue : plan.issues)
            {
                output << "  - " << issue.code << ": " << issue.message << '\n';
            }
        }
        return output.str();
    }

} // namespace us4::core
