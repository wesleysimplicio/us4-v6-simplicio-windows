#include "us4/runtime/backends/cpu_avx/blocked_matmul.h"

#include "us4/runtime/backends/cpu_avx/avx2_matmul.h"
#include "us4/runtime/backends/cpu_avx/avx512_matmul.h"
#include "us4/runtime/backends/onednn/block_gemm_contract.h"

#include <algorithm>
#include <cstdlib>
#if defined(_MSC_VER)
#include <intrin.h>
#endif
#include <stdexcept>

namespace us4::runtime::backends::cpu_avx
{
    namespace
    {

        std::size_t MinBlock(std::size_t remaining, std::size_t preferred) noexcept
        {
            return std::min(remaining, preferred);
        }

        struct HostVectorFlags
        {
            bool avx2 = false;
            bool avx512 = false;
        };

        HostVectorFlags DetectHostVectorFlags() noexcept
        {
#if defined(_MSC_VER) && (defined(_M_X64) || defined(_M_IX86))
            int registers[4] = {};
            __cpuidex(registers, 1, 0);

            const bool osxsave = (registers[2] & (1 << 27)) != 0;
            const bool avx = (registers[2] & (1 << 28)) != 0;
            if (!osxsave || !avx)
            {
                return {};
            }

            const unsigned __int64 xcr0 = _xgetbv(0);
            const bool ymmState = (xcr0 & 0x6U) == 0x6U;
            if (!ymmState)
            {
                return {};
            }

            __cpuidex(registers, 7, 0);
            HostVectorFlags flags;
            flags.avx2 = (registers[1] & (1 << 5)) != 0;

            const bool zmmState = (xcr0 & 0xE6U) == 0xE6U;
            flags.avx512 = zmmState && (registers[1] & (1 << 16)) != 0;
            return flags;
#else
            return {};
#endif
        }

        us4::core::Tensor ScalarBlockedMatMul(const us4::core::Tensor& left,
                                              const us4::core::Tensor& right,
                                              const BlockedMatMulPlan& plan)
        {
            const std::size_t rows = left.Dim(0);
            const std::size_t inner = left.Dim(1);
            const std::size_t cols = right.Dim(1);

            us4::core::Tensor output({rows, cols}, left.DataType());
            output.Fill(0.0F);

            const float* leftData = left.Data();
            const float* rightData = right.Data();
            float* outputData = output.Data();

            for (std::size_t rowBlockBegin = 0; rowBlockBegin < rows;
                 rowBlockBegin += plan.tile.rowBlock)
            {
                const std::size_t rowBlock =
                    MinBlock(rows - rowBlockBegin, std::max<std::size_t>(plan.tile.rowBlock, 1U));
                for (std::size_t colBlockBegin = 0; colBlockBegin < cols;
                     colBlockBegin += plan.tile.colBlock)
                {
                    const std::size_t colBlock = MinBlock(
                        cols - colBlockBegin, std::max<std::size_t>(plan.tile.colBlock, 1U));
                    for (std::size_t depthBlockBegin = 0; depthBlockBegin < inner;
                         depthBlockBegin += plan.tile.depthBlock)
                    {
                        const std::size_t depthBlock =
                            MinBlock(inner - depthBlockBegin,
                                     std::max<std::size_t>(plan.tile.depthBlock, 1U));
                        const std::vector<float> packedRight =
                            plan.packRightHandSide
                                ? PackRightHandSidePanel(right, depthBlockBegin, depthBlock,
                                                         colBlockBegin, colBlock)
                                : std::vector<float>();

                        for (std::size_t row = 0; row < rowBlock; ++row)
                        {
                            const std::size_t leftBase = (rowBlockBegin + row) * inner;
                            float* outputRow = outputData + ((rowBlockBegin + row) * cols) +
                                               colBlockBegin;
                            for (std::size_t depth = 0; depth < depthBlock; ++depth)
                            {
                                const float leftValue = leftData[leftBase + depthBlockBegin + depth];
                                const std::size_t packedBase = depth * colBlock;
                                const std::size_t rightBase =
                                    (depthBlockBegin + depth) * cols + colBlockBegin;
                                for (std::size_t col = 0; col < colBlock; ++col)
                                {
                                    const float rightValue =
                                        plan.packRightHandSide
                                            ? packedRight[packedBase + col]
                                            : rightData[rightBase + col];
                                    outputRow[col] += leftValue * rightValue;
                                }
                            }
                        }
                    }
                }
            }

            return output;
        }

    } // namespace

    BlockedMatMulPlan MakeReferenceMatMulPlan(std::size_t rows, std::size_t inner, std::size_t cols,
                                              const CpuKernelProfile* profile) noexcept
    {
        const CpuKernelProfile fallbackProfile = BuildKernelProfile(DefaultReferenceCapabilities());
        const CpuKernelProfile& selectedProfile = profile != nullptr ? *profile : fallbackProfile;

        BlockedMatMulPlan plan;
        plan.tile.rowBlock = std::max<std::size_t>(selectedProfile.matMul.rows, 4U);
        plan.tile.colBlock = std::max<std::size_t>(selectedProfile.matMul.cols, 8U);
        plan.tile.depthBlock = std::max<std::size_t>(selectedProfile.matMul.depth, 32U);
        plan.registerLaneWidth = std::max<std::size_t>(selectedProfile.matMul.laneWidth, 1U);
        plan.packRightHandSide = cols >= 8U;
        plan.useOneDnnFriendlyBlocking = selectedProfile.prefersOneDnnBlockGemm;
        plan.kernelTag = selectedProfile.kernelTag + "-reference";

        const onednn::OneDnnMatMulPlan oneDnnPlan = onednn::BuildOneDnnMatMulPlan(
            onednn::OneDnnMatMulDescriptor{
                rows,
                inner,
                cols,
                onednn::OneDnnDataType::kFloat32,
                onednn::OneDnnDataType::kFloat32,
                onednn::OneDnnDataType::kFloat32,
            },
            onednn::OneDnnMatMulPreference{
                selectedProfile.level == CpuInstructionSetLevel::kAmx,
                selectedProfile.level == CpuInstructionSetLevel::kAvx512 ||
                    selectedProfile.level == CpuInstructionSetLevel::kAmx,
                true,
                2048U * 1024U,
                8U,
            });
        if (oneDnnPlan.valid)
        {
            plan.tile.rowBlock = std::max(plan.tile.rowBlock, oneDnnPlan.rowBlock);
            plan.tile.colBlock = std::max(plan.tile.colBlock, oneDnnPlan.colBlock);
            plan.tile.depthBlock = std::max(plan.tile.depthBlock, oneDnnPlan.depthBlock);
            plan.packRightHandSide = plan.packRightHandSide || oneDnnPlan.packWeights;
            plan.useOneDnnFriendlyBlocking = true;
        }

        return plan;
    }

    std::vector<float> PackRightHandSidePanel(const us4::core::Tensor& right,
                                              const std::size_t depthBegin,
                                              const std::size_t depthBlock,
                                              const std::size_t colBegin,
                                              const std::size_t colBlock)
    {
        if (right.Rank() != 2U)
        {
            throw std::invalid_argument("PackRightHandSidePanel expects rank-2 right tensor.");
        }

        std::vector<float> packed(depthBlock * colBlock, 0.0F);
        for (std::size_t depth = 0; depth < depthBlock; ++depth)
        {
            for (std::size_t col = 0; col < colBlock; ++col)
            {
                const std::size_t depthIndex = depthBegin + depth;
                const std::size_t colIndex = colBegin + col;
                if (depthIndex < right.Dim(0) && colIndex < right.Dim(1))
                {
                    packed[(depth * colBlock) + col] = right.At({depthIndex, colIndex});
                }
            }
        }

        return packed;
    }

    us4::core::Tensor BlockedMatMul(const us4::core::Tensor& left, const us4::core::Tensor& right,
                                    const BlockedMatMulPlan& plan)
    {
        if (left.Rank() != 2U || right.Rank() != 2U || left.Dim(1) != right.Dim(0))
        {
            throw std::invalid_argument("BlockedMatMul expects [M, K] x [K, N] tensors.");
        }

        if (UseAvx512MatMulKernel(plan))
        {
            return Avx512BlockedMatMul(left, right, plan);
        }

        if (UseAvx2MatMulKernel(plan))
        {
            return Avx2BlockedMatMul(left, right, plan);
        }

        return ScalarBlockedMatMul(left, right, plan);
    }

    bool HostSupportsAvx2MatMul() noexcept
    {
        return DetectHostVectorFlags().avx2;
    }

    bool HostSupportsAvx512MatMul() noexcept
    {
        return DetectHostVectorFlags().avx512;
    }

    bool UseAvx2MatMulKernel(const BlockedMatMulPlan& plan) noexcept
    {
        return HostSupportsAvx2MatMul() &&
               (plan.registerLaneWidth >= 8U || plan.kernelTag.find("avx2") != std::string::npos);
    }

    bool UseAvx512MatMulKernel(const BlockedMatMulPlan& plan) noexcept
    {
        return HostSupportsAvx512MatMul() &&
               (plan.registerLaneWidth >= 16U || plan.kernelTag.find("avx512") != std::string::npos ||
                plan.kernelTag.find("amx") != std::string::npos);
    }

} // namespace us4::runtime::backends::cpu_avx
