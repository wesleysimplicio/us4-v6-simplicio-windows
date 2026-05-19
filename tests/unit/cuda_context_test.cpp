#include "us4/runtime/backends/cuda/cuda_backend.h"
#include "us4/runtime/backends/cuda/cuda_context.h"

#include <gtest/gtest.h>

namespace us4::runtime::tests
{
    namespace
    {
        constexpr std::size_t kGiB = 1024ULL * 1024ULL * 1024ULL;

        us4::runtime::backends::HardwareCapabilities MakeCudaCapabilities()
        {
            using namespace us4::runtime::backends;

            HardwareCapabilities capabilities;
            capabilities.hasCuda = true;
            capabilities.primaryAdapterVendor = BackendVendor::kNvidia;
            capabilities.primaryAdapterClass = DeviceClass::kDiscreteGpu;
            capabilities.primaryAdapterName = "GeForce RTX 4090";
            capabilities.discreteGpuCount = 1U;
            capabilities.hasUnifiedMemory = true;
            capabilities.budget.hostBytes = 64ULL * kGiB;
            capabilities.budget.deviceBytes = 24ULL * kGiB;
            capabilities.budget.storageBytes = 2ULL * 1024ULL * kGiB;
            return capabilities;
        }

        us4::runtime::backends::SessionRequest MakeCudaRequest()
        {
            using namespace us4::runtime::backends;

            SessionRequest request;
            request.modelId = "qwen-0.5b";
            request.mode = RuntimeMode::kBalanced;
            request.maxContextTokens = 4096U;
            request.maxGenerationTokens = 64U;
            request.preferMaxThroughput = true;
            request.preferLowLatency = false;
            request.seed = 77U;
            return request;
        }

        TEST(CudaContextTest, StreamIsolationKeepsPrefillDecodeAndTransferDistinct)
        {
            using namespace us4::runtime::backends::cuda;

            const auto plan =
                CudaBackend::BuildExecutionPlan(MakeCudaRequest(), MakeCudaCapabilities());
            auto context = CudaContext::Create(plan);

            const auto prefillA = context.AcquirePrefillStream();
            const auto prefillB = context.AcquirePrefillStream();
            const auto decode = context.AcquireDecodeStream();
            const auto transfer = context.AcquireTransferStream();

            EXPECT_NE(prefillA.id, prefillB.id);
            EXPECT_NE(prefillA.id, decode.id);
            EXPECT_NE(prefillA.id, transfer.id);
            EXPECT_TRUE(transfer.dedicatedTransfer);
            EXPECT_EQ(context.ActiveStreamCount(), 4U);

            EXPECT_TRUE(context.ReleaseStream(prefillA).ok);
            EXPECT_TRUE(context.ReleaseStream(prefillB).ok);
            EXPECT_TRUE(context.ReleaseStream(decode).ok);
            EXPECT_TRUE(context.ReleaseStream(transfer).ok);
            EXPECT_EQ(context.ActiveStreamCount(), 0U);
        }

        TEST(CudaContextTest, PoolReuseRecyclesReleasedScratchBlocks)
        {
            using namespace us4::runtime::backends::cuda;

            const auto plan =
                CudaBackend::BuildExecutionPlan(MakeCudaRequest(), MakeCudaCapabilities());
            auto context = CudaContext::Create(plan);

            const auto first = context.AllocateAsync(8ULL * 1024ULL * 1024ULL, "prefill-scratch");
            EXPECT_TRUE(context.ReleaseAllocation(first).ok);
            const auto second = context.AllocateAsync(4ULL * 1024ULL * 1024ULL, "decode-scratch");

            EXPECT_EQ(first.id, second.id);
            EXPECT_TRUE(second.reusedFromPool);
            EXPECT_GE(context.PoolCapacityBytes(), 8ULL * 1024ULL * 1024ULL);
        }

        TEST(CudaContextTest, GraphCaptureReplayIsDeterministicAndAvoidsDeviceSynchronizeMarker)
        {
            using namespace us4::runtime::backends::cuda;

            const auto plan =
                CudaBackend::BuildExecutionPlan(MakeCudaRequest(), MakeCudaCapabilities());
            ASSERT_TRUE(plan.graph.enableGraphCapture);

            auto context = CudaContext::Create(plan);
            const auto executable =
                context.CaptureGraph("decode-step", {"matmul", "rmsnorm", "softmax", "sample"});
            ASSERT_TRUE(executable.has_value());

            const auto replayA = context.ReplayGraph(*executable, 8U, 19U);
            const auto replayB = context.ReplayGraph(*executable, 8U, 19U);

            EXPECT_EQ(replayA.tokens, replayB.tokens);
            EXPECT_EQ(context.GraphReplayCount(), 2U);

            for (const auto& line : context.ExecutionTrace())
            {
                EXPECT_EQ(line.find("cudaDeviceSynchronize"), std::string::npos);
            }
        }

        TEST(CudaContextTest, CaptureGraphReturnsNulloptWhenGraphsAreDisabled)
        {
            using namespace us4::runtime::backends;
            using namespace us4::runtime::backends::cuda;

            auto request = MakeCudaRequest();
            request.preferMaxThroughput = false;
            request.preferLowLatency = true;
            request.mode = RuntimeMode::kUltraLow;

            const auto plan = CudaBackend::BuildExecutionPlan(request, MakeCudaCapabilities());
            ASSERT_FALSE(plan.graph.enableGraphCapture);

            auto context = CudaContext::Create(plan);
            const auto executable = context.CaptureGraph("decode-step", {"matmul"});
            EXPECT_FALSE(executable.has_value());
        }

        TEST(CudaContextTest, UnknownReleasesReturnStructuredErrors)
        {
            using namespace us4::runtime::backends::cuda;

            const auto plan =
                CudaBackend::BuildExecutionPlan(MakeCudaRequest(), MakeCudaCapabilities());
            auto context = CudaContext::Create(plan);

            const auto streamStatus =
                context.ReleaseStream(CudaStreamHandle{.id = 999U, .label = "ghost-stream"});
            EXPECT_FALSE(streamStatus.ok);
            EXPECT_EQ(streamStatus.code, "cuda.stream.unknown");

            const auto allocationStatus = context.ReleaseAllocation(
                CudaPoolAllocation{.id = 777U, .tag = "ghost-allocation"});
            EXPECT_FALSE(allocationStatus.ok);
            EXPECT_EQ(allocationStatus.code, "cuda.pool.unknown_allocation");
        }
    } // namespace
} // namespace us4::runtime::tests
