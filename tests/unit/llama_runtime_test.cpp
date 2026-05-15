#include "runtime/core/cpu_scalar_runtime.h"
#include "runtime/core/gqa_attention.h"
#include "runtime/core/rope.h"
#include "runtime/core/runtime_context.h"
#include "us4/runtime/adapters/dense_adapter_base.h"
#include "us4/runtime/adapters/scalar_family_adapters.h"
#include "us4/runtime/backends/runtime_types.h"
#include "us4/runtime/benchmarks/benchmark_registry.h"

#include <algorithm>
#include <gtest/gtest.h>

namespace us4::runtime::tests
{
    namespace
    {

        TEST(LlamaRuntimeTest, RopeKeepsVectorAtPositionZero)
        {
            us4::core::Tensor input({1U, 4U});
            input.At({0U, 0U}) = 1.0F;
            input.At({0U, 1U}) = 2.0F;
            input.At({0U, 2U}) = 3.0F;
            input.At({0U, 3U}) = 4.0F;

            const auto output = us4::core::ApplyRoPE(
                input, 0U, us4::core::RopeConfig{.scalingFactor = 1.0F});

            EXPECT_NEAR(output.At({0U, 0U}), 1.0F, 1e-5F);
            EXPECT_NEAR(output.At({0U, 1U}), 2.0F, 1e-5F);
            EXPECT_NEAR(output.At({0U, 2U}), 3.0F, 1e-5F);
            EXPECT_NEAR(output.At({0U, 3U}), 4.0F, 1e-5F);
        }

        TEST(LlamaRuntimeTest, RopeScalingModesProduceDistinctRotations)
        {
            us4::core::Tensor input({1U, 4U});
            input.At({0U, 0U}) = 1.0F;
            input.At({0U, 1U}) = 0.0F;
            input.At({0U, 2U}) = 0.5F;
            input.At({0U, 3U}) = 0.5F;

            const auto linear = us4::core::ApplyRoPE(
                input, 8U,
                us4::core::RopeConfig{
                    .scalingFactor = 1.0F,
                    .scalingMode = us4::core::RopeScalingMode::kLinear,
                });
            const auto dynamic = us4::core::ApplyRoPE(
                input, 8U,
                us4::core::RopeConfig{
                    .scalingFactor = 1.0F,
                    .scalingMode = us4::core::RopeScalingMode::kDynamic,
                });
            const auto yarn = us4::core::ApplyRoPE(
                input, 8U,
                us4::core::RopeConfig{
                    .scalingFactor = 1.0F,
                    .yarnAttentionFactor = 2.0F,
                    .scalingMode = us4::core::RopeScalingMode::kYarn,
                });

            EXPECT_NE(linear.At({0U, 0U}), dynamic.At({0U, 0U}));
            EXPECT_NE(dynamic.At({0U, 0U}), yarn.At({0U, 0U}));
        }

        TEST(LlamaRuntimeTest, GqaExpandsGroupedHeadsToQueryHeadShape)
        {
            us4::core::Tensor kv({2U, 2U, 2U});
            kv.At({0U, 0U, 0U}) = 1.0F;
            kv.At({0U, 0U, 1U}) = 2.0F;
            kv.At({0U, 1U, 0U}) = 3.0F;
            kv.At({0U, 1U, 1U}) = 4.0F;
            kv.At({1U, 0U, 0U}) = 10.0F;
            kv.At({1U, 0U, 1U}) = 20.0F;
            kv.At({1U, 1U, 0U}) = 30.0F;
            kv.At({1U, 1U, 1U}) = 40.0F;

            const auto expanded = us4::core::ExpandGroupedHeads(
                kv, us4::core::GqaConfig{
                        .queryHeads = 4U,
                        .kvHeads = 2U,
                        .sequenceLength = 2U,
                        .headDim = 2U,
                    });

            ASSERT_EQ(expanded.Shape(), std::vector<std::size_t>({4U, 2U, 2U}));
            EXPECT_EQ(expanded.At({0U, 1U, 0U}), 3.0F);
            EXPECT_EQ(expanded.At({1U, 1U, 1U}), 4.0F);
            EXPECT_EQ(expanded.At({2U, 0U, 0U}), 10.0F);
            EXPECT_EQ(expanded.At({3U, 1U, 1U}), 40.0F);
        }

        TEST(LlamaRuntimeTest, LlamaScalarAdapterRoundTripsPromptTokens)
        {
            auto adapter = us4::runtime::adapters::CreateLlamaScalarAdapter();
            auto* dense = dynamic_cast<us4::runtime::adapters::DenseAdapterBase*>(adapter.get());

            ASSERT_NE(dense, nullptr);
            const std::string prompt = "llama runtime";
            const auto tokens = dense->TokenizePrompt(prompt);

            EXPECT_EQ(dense->DetokenizePromptTokens(tokens), prompt);
            EXPECT_EQ(tokens.size(), prompt.size());
        }

        TEST(LlamaRuntimeTest, CpuScalarRunServesLlamaPlan)
        {
            us4::runtime::backends::HardwareCapabilities capabilities{};
            capabilities.hasAvx2 = true;
            capabilities.budget.hostBytes = 24ULL * 1024ULL * 1024ULL * 1024ULL;

            us4::runtime::backends::SessionRequest request{};
            request.modelId = "llama-3.2-3b";
            request.mode = us4::runtime::backends::RuntimeMode::kCpuOnly;
            request.preferredBackend = "cpu";
            request.maxGenerationTokens = 5U;

            const us4::core::RuntimePlan plan = us4::core::RuntimeContext::BuildPlan(request, capabilities);
            const auto runResult = us4::core::ExecuteCpuScalarRun(plan, "hello llama");

            ASSERT_TRUE(runResult.ok);
            EXPECT_EQ(plan.model.adapterId, "dense-llama");
            EXPECT_FALSE(runResult.report.generatedText.empty());
            EXPECT_EQ(runResult.report.kvStats.segmentCount, 1U);
        }

        TEST(LlamaRuntimeTest, BenchmarkRegistryIncludesLlamaCpuBaseline)
        {
            const auto cpuCases =
                us4::runtime::benchmarks::BenchmarkRegistry::CasesForBackend("cpu");

            EXPECT_TRUE(std::any_of(cpuCases.begin(), cpuCases.end(),
                                    [](const auto& benchmark)
                                    {
                                        return benchmark.name == "dense_baseline_llama_cpu_only" &&
                                               benchmark.adapterId == "dense-llama" &&
                                               benchmark.modelId == "llama-3.2-3b";
                                    }));
        }

    } // namespace
} // namespace us4::runtime::tests
