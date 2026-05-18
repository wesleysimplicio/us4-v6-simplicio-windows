#include "runtime/core/cpu_scalar_runtime.h"
#include "runtime/core/runtime_context.h"
#include "us4/runtime/adapters/moe_model_loader.h"
#include "us4/runtime/backends/runtime_types.h"
#include "us4/runtime/moe/expert_pager.h"
#include "us4/runtime/moe/expert_router.h"

#include <array>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>

namespace us4::runtime::tests
{
    namespace
    {
        namespace fs = std::filesystem;

        class ScopedMoeFixtureDirectory
        {
          public:
            ScopedMoeFixtureDirectory()
            {
                path_ = fs::temp_directory_path() /
                        fs::path("us4-moe-loader-" + std::to_string(counter_++));
                fs::create_directories(path_);
            }

            ~ScopedMoeFixtureDirectory()
            {
                std::error_code ignored;
                fs::remove_all(path_, ignored);
            }

            [[nodiscard]] const fs::path& Path() const
            {
                return path_;
            }

          private:
            fs::path path_;
            inline static std::size_t counter_ = 0U;
        };

        template <typename TValue> void WriteBinaryValue(std::ofstream& stream, const TValue value)
        {
            stream.write(reinterpret_cast<const char*>(&value),
                         static_cast<std::streamsize>(sizeof(TValue)));
        }

        void WriteSafeTensorsShard(const fs::path& path)
        {
            const std::string header = R"({
  "expert.weight": {"dtype":"F16","shape":[2,2],"data_offsets":[0,16]}
})";

            std::ofstream stream(path, std::ios::binary);
            ASSERT_TRUE(stream.is_open());
            const std::uint64_t headerSize = static_cast<std::uint64_t>(header.size());
            WriteBinaryValue(stream, headerSize);
            stream.write(header.data(), static_cast<std::streamsize>(header.size()));
            const std::array<char, 32> payload{};
            stream.write(payload.data(), static_cast<std::streamsize>(payload.size()));
        }

        void WriteBaseModelAsset(const fs::path& directory)
        {
            WriteSafeTensorsShard(directory / "model.safetensors");
        }

        TEST(MoeRuntimeTest, ExpertRouterBuildsTopKPlanAndEntropy)
        {
            moe::ExpertRouter router;
            const auto routes =
                router.BuildPlan("deepseek route window", moe::RoutingPolicy{
                                                              .topK = 3U,
                                                              .preferDeterministic = true,
                                                              .allowCrossDeviceExperts = true,
                                                          });
            const auto stats = router.Evaluate(routes);

            ASSERT_EQ(routes.size(), 3U);
            EXPECT_EQ(stats.hostRouteCount, 1U);
            EXPECT_EQ(stats.deviceRouteCount, 2U);
            EXPECT_GT(stats.entropy, 0.0F);
            EXPECT_GT(stats.loadBalanceLoss, 0.0F);
        }

        TEST(MoeRuntimeTest, ExpertPagerRebalancesHotToWarmAndCold)
        {
            moe::ExpertPager pager;
            pager.ConfigureBudget(moe::ExpertPagerBudget{
                .hotBytes = 1536U,
                .warmBytes = 1536U,
                .coldBytes = 4096U,
            });

            EXPECT_TRUE(pager.Touch(1U, 1536U, moe::ExpertResidency::kHot));
            EXPECT_TRUE(pager.Touch(2U, 1536U, moe::ExpertResidency::kHot));
            EXPECT_TRUE(pager.Touch(3U, 1536U, moe::ExpertResidency::kHot));

            const auto stats = pager.Stats();
            EXPECT_EQ(stats.hotExperts, 1U);
            EXPECT_EQ(stats.warmExperts, 1U);
            EXPECT_EQ(stats.coldExperts, 1U);
            EXPECT_GT(stats.evictionCount, 0U);
            EXPECT_GT(stats.coldOffloadCount, 0U);

            const auto coldEntry = pager.Find(1U);
            ASSERT_TRUE(coldEntry.has_value());
            EXPECT_EQ(coldEntry->residency, moe::ExpertResidency::kCold);
            EXPECT_TRUE(coldEntry->offloaded);
        }

        TEST(MoeRuntimeTest, ExpertPagerReloadsColdExpertsWithoutChangingPayloadChecksum)
        {
            moe::ExpertPager pager;
            pager.ConfigureBudget(moe::ExpertPagerBudget{
                .hotBytes = 1536U,
                .warmBytes = 1536U,
                .coldBytes = 4096U,
            });

            EXPECT_TRUE(pager.Touch(1U, 1536U, moe::ExpertResidency::kHot));
            EXPECT_TRUE(pager.Touch(2U, 1536U, moe::ExpertResidency::kHot));
            EXPECT_TRUE(pager.Touch(3U, 1536U, moe::ExpertResidency::kHot));

            const auto beforeReload = pager.Find(1U);
            ASSERT_TRUE(beforeReload.has_value());
            ASSERT_TRUE(beforeReload->offloaded);
            const auto expectedChecksum = beforeReload->payloadChecksum;
            const auto expectedResidentBytes = beforeReload->residentBytes;

            EXPECT_TRUE(pager.Reload(1U, moe::ExpertResidency::kHot));

            const auto afterReload = pager.Find(1U);
            ASSERT_TRUE(afterReload.has_value());
            EXPECT_FALSE(afterReload->offloaded);
            EXPECT_EQ(afterReload->payloadChecksum, expectedChecksum);
            EXPECT_EQ(afterReload->residentBytes, expectedResidentBytes);
            EXPECT_GT(afterReload->reloadCount, 0U);

            const auto stats = pager.Stats();
            EXPECT_GT(stats.reloadCount, 0U);
            EXPECT_GT(stats.coldOffloadCount, 0U);
        }

        TEST(MoeRuntimeTest, MoeModelLoaderMapsOnlyRequestedExpertShards)
        {
            ScopedMoeFixtureDirectory fixture;
            WriteSafeTensorsShard(fixture.Path() / "expert_000.safetensors");
            WriteSafeTensorsShard(fixture.Path() / "expert_001.safetensors");
            WriteSafeTensorsShard(fixture.Path() / "expert_002.safetensors");

            us4::runtime::adapters::MoeModelLoader loader;
            ASSERT_TRUE(loader.Open(fixture.Path().string()));
            EXPECT_EQ(loader.KnownExpertCount(), 3U);
            EXPECT_EQ(loader.MappedExpertCount(), 0U);

            const auto first = loader.LoadExpert(1U);
            ASSERT_TRUE(first.has_value());
            EXPECT_TRUE(loader.IsMapped(1U));
            EXPECT_EQ(loader.MappedExpertCount(), 1U);
            EXPECT_EQ(loader.MappedExpertIds(), (std::vector<std::size_t>{1U}));

            const auto repeat = loader.LoadExpert(1U);
            ASSERT_TRUE(repeat.has_value());
            EXPECT_EQ(loader.MappedExpertCount(), 1U);

            const auto second = loader.LoadExpert(2U);
            ASSERT_TRUE(second.has_value());
            EXPECT_EQ(loader.MappedExpertCount(), 2U);
            EXPECT_EQ(loader.MappedExpertIds(), (std::vector<std::size_t>{1U, 2U}));

            EXPECT_TRUE(loader.UnloadExpert(1U));
            EXPECT_FALSE(loader.IsMapped(1U));
            EXPECT_EQ(loader.MappedExpertIds(), (std::vector<std::size_t>{2U}));
        }

        TEST(MoeRuntimeTest, CpuScalarRunServesDeepSeekMoeScaffold)
        {
            us4::runtime::backends::HardwareCapabilities capabilities{};
            capabilities.hasAvx2 = true;
            capabilities.budget.hostBytes = 32ULL * 1024ULL * 1024ULL * 1024ULL;

            us4::runtime::backends::SessionRequest request{};
            request.modelId = "deepseek-r1-distill";
            request.mode = us4::runtime::backends::RuntimeMode::kCpuOnly;
            request.preferredBackend = "cpu";
            request.maxGenerationTokens = 4U;

            const us4::core::RuntimePlan plan =
                us4::core::RuntimeContext::BuildPlan(request, capabilities);
            const auto runResult = us4::core::ExecuteCpuScalarRun(plan, "deepseek moe check");

            ASSERT_TRUE(runResult.ok);
            EXPECT_EQ(plan.model.adapterId, "moe-deepseek");
            EXPECT_GT(runResult.report.moeRouteCount, 0U);
            EXPECT_GT(runResult.report.moeRouterEntropy, 0.0F);
            EXPECT_GT(runResult.report.moeLoadBalanceLoss, 0.0F);
            EXPECT_GT(runResult.report.telemetryEventCount, 3U);
        }

        TEST(MoeRuntimeTest, CpuScalarRunMapsOnlyReferencedExpertsWhenLazyMoeDirectoryExists)
        {
            ScopedMoeFixtureDirectory fixture;
            WriteBaseModelAsset(fixture.Path());
            WriteSafeTensorsShard(fixture.Path() / "expert_000.safetensors");
            WriteSafeTensorsShard(fixture.Path() / "expert_001.safetensors");
            WriteSafeTensorsShard(fixture.Path() / "expert_002.safetensors");

            us4::runtime::backends::HardwareCapabilities capabilities{};
            capabilities.hasAvx2 = true;
            capabilities.budget.hostBytes = 32ULL * 1024ULL * 1024ULL * 1024ULL;

            us4::runtime::backends::SessionRequest request{};
            request.modelId = "deepseek-r1-distill";
            request.mode = us4::runtime::backends::RuntimeMode::kCpuOnly;
            request.preferredBackend = "cpu";
            request.maxGenerationTokens = 4U;

            const us4::core::RuntimePlan plan =
                us4::core::RuntimeContext::BuildPlan(request, capabilities);
            const auto runResult = us4::core::ExecuteCpuScalarRun(plan, "deepseek lazy moe check",
                                                                  fixture.Path().string());

            ASSERT_TRUE(runResult.ok);
            EXPECT_EQ(runResult.report.moeRouteCount, 2U);
            EXPECT_EQ(runResult.report.moeMappedExpertCount, 2U);
            EXPECT_EQ(runResult.report.moeMappedExpertIds, (std::vector<std::size_t>{0U, 1U}));
        }

        TEST(MoeRuntimeTest, CpuScalarRunServesKimiMoeScaffold)
        {
            us4::runtime::backends::HardwareCapabilities capabilities{};
            capabilities.hasAvx2 = true;
            capabilities.budget.hostBytes = 32ULL * 1024ULL * 1024ULL * 1024ULL;

            us4::runtime::backends::SessionRequest request{};
            request.modelId = "kimi-k2";
            request.mode = us4::runtime::backends::RuntimeMode::kCpuOnly;
            request.preferredBackend = "cpu";
            request.maxGenerationTokens = 4U;

            const us4::core::RuntimePlan plan =
                us4::core::RuntimeContext::BuildPlan(request, capabilities);
            const auto runResult = us4::core::ExecuteCpuScalarRun(plan, "kimi moe check");

            ASSERT_TRUE(runResult.ok);
            EXPECT_EQ(plan.model.adapterId, "moe-kimi");
            EXPECT_GT(runResult.report.moeRouteCount, 0U);
            EXPECT_GT(runResult.report.moeRouterEntropy, 0.0F);
            EXPECT_GT(runResult.report.moeLoadBalanceLoss, 0.0F);
            EXPECT_GT(runResult.report.moePrefetchTelemetry.hitRatio, 0.0F);
            EXPECT_GT(runResult.report.telemetryEventCount, 3U);
        }

    } // namespace
} // namespace us4::runtime::tests
