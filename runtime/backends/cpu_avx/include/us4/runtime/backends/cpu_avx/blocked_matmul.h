#pragma once

#include "runtime/core/tensor.h"
#include "us4/runtime/backends/cpu_avx/kernel_profile.h"

#include <cstddef>
#include <string>
#include <vector>

namespace us4::runtime::backends::cpu_avx
{

    struct BlockedMatMulTile
    {
        std::size_t rowBlock = 32;
        std::size_t colBlock = 32;
        std::size_t depthBlock = 64;
    };

    struct BlockedMatMulPlan
    {
        BlockedMatMulTile tile;
        std::size_t registerLaneWidth = 1;
        bool packRightHandSide = true;
        bool useOneDnnFriendlyBlocking = false;
        std::string kernelTag = "scalar-blocked";
    };

    [[nodiscard]] BlockedMatMulPlan
    MakeReferenceMatMulPlan(std::size_t rows, std::size_t inner, std::size_t cols,
                            const CpuKernelProfile* profile = nullptr) noexcept;
    [[nodiscard]] std::vector<float>
    PackRightHandSidePanel(const us4::core::Tensor& right, std::size_t depthBegin,
                           std::size_t depthBlock, std::size_t colBegin, std::size_t colBlock);
    [[nodiscard]] us4::core::Tensor BlockedMatMul(const us4::core::Tensor& left,
                                                  const us4::core::Tensor& right,
                                                  const BlockedMatMulPlan& plan);
    [[nodiscard]] bool HostSupportsAvx2MatMul() noexcept;
    [[nodiscard]] bool HostSupportsAvx512MatMul() noexcept;
    [[nodiscard]] bool UseAvx2MatMulKernel(const BlockedMatMulPlan& plan) noexcept;
    [[nodiscard]] bool UseAvx512MatMulKernel(const BlockedMatMulPlan& plan) noexcept;

} // namespace us4::runtime::backends::cpu_avx
