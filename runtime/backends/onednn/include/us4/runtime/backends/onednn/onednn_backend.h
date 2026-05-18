#pragma once

#include "runtime/core/tensor.h"
#include "us4/runtime/backends/cpu_avx/blocked_matmul.h"
#include "us4/runtime/backends/cpu_avx/kernel_profile.h"
#include "us4/runtime/backends/onednn/block_gemm_contract.h"

#include <cstddef>
#include <string>

namespace us4::runtime::backends::onednn
{

    struct OneDnnBackendOptions
    {
        bool allowAmx = true;
        bool allowAvx512 = true;
        bool preferPersistentCache = true;
        std::size_t l2BytesPerCore = 2048U * 1024U;
        std::size_t threadCountHint = 8U;
    };

    struct OneDnnMatMulResult
    {
        us4::core::Tensor output;
        OneDnnMatMulPlan plan;
        cpu_avx::BlockedMatMulPlan blockedPlan;
    };

    class OneDnnBackend
    {
      public:
        explicit OneDnnBackend(OneDnnBackendOptions options = {}) noexcept;

        [[nodiscard]] OneDnnMatMulPlan
        PlanMatMul(const us4::core::Tensor& left, const us4::core::Tensor& right,
                   const cpu_avx::CpuKernelProfile* profile = nullptr) const;
        [[nodiscard]] OneDnnMatMulResult
        ExecuteMatMul(const us4::core::Tensor& left, const us4::core::Tensor& right,
                      const cpu_avx::CpuKernelProfile* profile = nullptr) const;

      private:
        OneDnnBackendOptions options_;
    };

} // namespace us4::runtime::backends::onednn
