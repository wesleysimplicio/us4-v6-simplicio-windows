#include "us4/profiles/profile_catalog.h"

namespace us4::profiles
{
    namespace
    {
        constexpr std::size_t kGiB = 1024ULL * 1024ULL * 1024ULL;
    }

    std::vector<RuntimeProfile> ProfileCatalog::Defaults()
    {
        using us4::runtime::backends::RuntimeMode;

        return {
            RuntimeProfile{.id = "full",
                           .displayName = "Full",
                           .mode = RuntimeMode::kFull,
                           .preferredBackend = "cuda",
                           .targetContextTokens = 16384,
                           .targetBatchSize = 8,
                           .enableKvTiering = true,
                           .enableSpeculative = true,
                           .enableMoE = true,
                           .enableContinuousBatching = true},
            RuntimeProfile{.id = "balanced",
                           .displayName = "Balanced",
                           .mode = RuntimeMode::kBalanced,
                           .preferredBackend = "auto",
                           .targetContextTokens = 8192,
                           .targetBatchSize = 4,
                           .enableKvTiering = true,
                           .enableSpeculative = true,
                           .enableMoE = true,
                           .enableContinuousBatching = true},
            RuntimeProfile{.id = "degraded",
                           .displayName = "Degraded",
                           .mode = RuntimeMode::kDegraded,
                           .preferredBackend = "directml",
                           .targetContextTokens = 6144,
                           .targetBatchSize = 4,
                           .enableKvTiering = true,
                           .enableSpeculative = false,
                           .enableMoE = true,
                           .enableContinuousBatching = false},
            RuntimeProfile{.id = "ultra-low",
                           .displayName = "Ultra Low",
                           .mode = RuntimeMode::kUltraLow,
                           .preferredBackend = "directml",
                           .targetContextTokens = 4096,
                           .targetBatchSize = 2,
                           .enableKvTiering = true,
                           .enableSpeculative = false,
                           .enableMoE = false,
                           .enableContinuousBatching = false},
            RuntimeProfile{.id = "micro",
                           .displayName = "Micro",
                           .mode = RuntimeMode::kMicro,
                           .preferredBackend = "vulkan",
                           .targetContextTokens = 4096,
                           .targetBatchSize = 2,
                           .enableKvTiering = true,
                           .enableSpeculative = false,
                           .enableMoE = false,
                           .enableContinuousBatching = false},
            RuntimeProfile{.id = "nano",
                           .displayName = "Nano",
                           .mode = RuntimeMode::kNano,
                           .preferredBackend = "vulkan",
                           .targetContextTokens = 2048,
                           .targetBatchSize = 1,
                           .enableKvTiering = false,
                           .enableSpeculative = false,
                           .enableMoE = false,
                           .enableContinuousBatching = false},
            RuntimeProfile{.id = "cpu-only",
                           .displayName = "CPU Only",
                           .mode = RuntimeMode::kCpuOnly,
                           .preferredBackend = "cpu-avx2",
                           .targetContextTokens = 2048,
                           .targetBatchSize = 1,
                           .enableKvTiering = false,
                           .enableSpeculative = false,
                           .enableMoE = false,
                           .enableContinuousBatching = false},
        };
    }

    std::optional<RuntimeProfile> ProfileCatalog::FindById(const std::string& id)
    {
        for (const RuntimeProfile& profile : Defaults())
        {
            if (profile.id == id)
            {
                return profile;
            }
        }
        return std::nullopt;
    }

    std::string
    ProfileCatalog::RecommendId(const us4::runtime::backends::HardwareCapabilities& capabilities)
    {
        if (capabilities.budget.hostBytes > 0 && capabilities.budget.hostBytes <= 8ULL * kGiB)
        {
            return capabilities.hasCuda || capabilities.hasDirectMl || capabilities.hasVulkan ||
                           capabilities.hasNpu
                       ? "micro"
                       : "nano";
        }

        if (capabilities.hasCuda)
        {
            return capabilities.budget.deviceBytes >= 24ULL * kGiB ? "full" : "balanced";
        }
        if (capabilities.hasNpu && !capabilities.hasCuda && !capabilities.hasDirectMl &&
            !capabilities.hasVulkan)
        {
            return "micro";
        }
        if (capabilities.hasDirectMl)
        {
            return capabilities.primaryAdapterClass ==
                               us4::runtime::backends::DeviceClass::kIntegratedGpu ||
                           capabilities.budget.deviceBytes < 8ULL * kGiB
                       ? "ultra-low"
                       : "degraded";
        }
        if (capabilities.hasVulkan)
        {
            return capabilities.budget.deviceBytes >= 8ULL * kGiB ? "micro" : "nano";
        }
        if (capabilities.hasAmx || capabilities.hasAvx512)
        {
            return "degraded";
        }
        return "cpu-only";
    }

} // namespace us4::profiles
