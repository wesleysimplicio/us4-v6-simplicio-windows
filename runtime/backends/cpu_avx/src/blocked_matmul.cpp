#include "us4/runtime/backends/cpu_avx/blocked_matmul.h"

#include "us4/runtime/backends/onednn/block_gemm_contract.h"

#include <algorithm>
#include <stdexcept>

namespace us4::runtime::backends::cpu_avx
{
    namespace
    {

        std::size_t MinBlock(std::size_t remaining, std::size_t preferred) noexcept
        {
            return std::min(remaining, preferred);
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

        const std::size_t rows = left.Dim(0);
        const std::size_t inner = left.Dim(1);
        const std::size_t cols = right.Dim(1);

        us4::core::Tensor output({rows, cols}, left.DataType());
        output.Fill(0.0F);

        for (std::size_t rowBlockBegin = 0; rowBlockBegin < rows;
             rowBlockBegin += plan.tile.rowBlock)
        {
            const std::size_t rowBlock =
                MinBlock(rows - rowBlockBegin, std::max<std::size_t>(plan.tile.rowBlock, 1U));
            for (std::size_t colBlockBegin = 0; colBlockBegin < cols;
                 colBlockBegin += plan.tile.colBlock)
            {
                const std::size_t colBlock =
                    MinBlock(cols - colBlockBegin, std::max<std::size_t>(plan.tile.colBlock, 1U));
                for (std::size_t depthBlockBegin = 0; depthBlockBegin < inner;
                     depthBlockBegin += plan.tile.depthBlock)
                {
                    const std::size_t depthBlock = MinBlock(
                        inner - depthBlockBegin, std::max<std::size_t>(plan.tile.depthBlock, 1U));
                    const std::vector<float> packedRight =
                        plan.packRightHandSide
                            ? PackRightHandSidePanel(right, depthBlockBegin, depthBlock,
                                                     colBlockBegin, colBlock)
                            : std::vector<float>();

                    for (std::size_t row = 0; row < rowBlock; ++row)
                    {
                        for (std::size_t depth = 0; depth < depthBlock; ++depth)
                        {
                            const float leftValue =
                                left.At({rowBlockBegin + row, depthBlockBegin + depth});
                            const std::size_t packedBase = depth * colBlock;
                            for (std::size_t col = 0; col < colBlock; ++col)
                            {
                                const float rightValue =
                                    plan.packRightHandSide
                                        ? packedRight[packedBase + col]
                                        : right.At({depthBlockBegin + depth, colBlockBegin + col});
                                output.At({rowBlockBegin + row, colBlockBegin + col}) +=
                                    leftValue * rightValue;
                            }
                        }
                    }
                }
            }
        }

        return output;
    }

} // namespace us4::runtime::backends::cpu_avx
