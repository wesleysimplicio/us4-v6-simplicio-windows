#include "us4/runtime/backends/cpu_avx/avx2_matmul.h"

#include <algorithm>
#include <immintrin.h>
#include <stdexcept>
#include <vector>

namespace us4::runtime::backends::cpu_avx
{
    namespace
    {

        std::size_t MinBlock(std::size_t remaining, std::size_t preferred) noexcept
        {
            return std::min(remaining, preferred);
        }

    } // namespace

    us4::core::Tensor Avx2BlockedMatMul(const us4::core::Tensor& left, const us4::core::Tensor& right,
                                        const BlockedMatMulPlan& plan)
    {
        if (left.Rank() != 2U || right.Rank() != 2U || left.Dim(1) != right.Dim(0))
        {
            throw std::invalid_argument("Avx2BlockedMatMul expects [M, K] x [K, N] tensors.");
        }

        const std::size_t rows = left.Dim(0);
        const std::size_t inner = left.Dim(1);
        const std::size_t cols = right.Dim(1);
        const float* leftData = left.Data();
        const float* rightData = right.Data();

        us4::core::Tensor output({rows, cols}, left.DataType());
        output.Fill(0.0F);
        float* outputData = output.Data();

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
                        const std::size_t leftBase = (rowBlockBegin + row) * inner;
                        float* outputRow = outputData + ((rowBlockBegin + row) * cols) +
                                           colBlockBegin;
                        std::size_t col = 0;
                        for (; col + 8U <= colBlock; col += 8U)
                        {
                            __m256 acc = _mm256_loadu_ps(outputRow + col);
                            for (std::size_t depth = 0; depth < depthBlock; ++depth)
                            {
                                const __m256 leftBroadcast = _mm256_set1_ps(
                                    leftData[leftBase + depthBlockBegin + depth]);
                                const float* rightPanel = plan.packRightHandSide
                                                              ? packedRight.data() + (depth * colBlock)
                                                              : rightData +
                                                                    ((depthBlockBegin + depth) * cols) +
                                                                    colBlockBegin;
                                const __m256 rhs = _mm256_loadu_ps(rightPanel + col);
                                acc = _mm256_add_ps(acc, _mm256_mul_ps(leftBroadcast, rhs));
                            }
                            _mm256_storeu_ps(outputRow + col, acc);
                        }

                        for (; col < colBlock; ++col)
                        {
                            float acc = outputRow[col];
                            for (std::size_t depth = 0; depth < depthBlock; ++depth)
                            {
                                const float rhs =
                                    plan.packRightHandSide
                                        ? packedRight[(depth * colBlock) + col]
                                        : rightData[((depthBlockBegin + depth) * cols) +
                                                    colBlockBegin + col];
                                acc += leftData[leftBase + depthBlockBegin + depth] * rhs;
                            }
                            outputRow[col] = acc;
                        }
                    }
                }
            }
        }

        return output;
    }

} // namespace us4::runtime::backends::cpu_avx
