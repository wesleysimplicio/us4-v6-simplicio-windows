#include "us4/runtime/backends/cpu_avx/scalar_matmul.h"

#include <sstream>
#include <stdexcept>

namespace us4::runtime::backends::cpu_avx
{

    MatMulValidation ValidateScalarMatMul(const us4::core::Tensor& left,
                                          const us4::core::Tensor& right)
    {
        if (left.Rank() != 2 || right.Rank() != 2)
        {
            return {false, "ScalarMatMul expects rank-2 tensors [M, K] x [K, N]."};
        }

        if (left.Dim(1) != right.Dim(0))
        {
            std::ostringstream error;
            error << "Inner dimensions do not match for matmul: left=" << left.DebugShape()
                  << " right=" << right.DebugShape();
            return {false, error.str()};
        }

        return {true, {}};
    }

    BlockedMatMulPlan ExplainScalarMatMulPlan(const us4::core::Tensor& left,
                                              const us4::core::Tensor& right)
    {
        const MatMulValidation validation = ValidateScalarMatMul(left, right);
        if (!validation.ok)
        {
            throw std::invalid_argument(validation.message);
        }

        const CpuKernelProfile profile = BuildKernelProfile(DefaultReferenceCapabilities());
        return MakeReferenceMatMulPlan(left.Dim(0), left.Dim(1), right.Dim(1), &profile);
    }

    us4::core::Tensor ScalarMatMul(const us4::core::Tensor& left, const us4::core::Tensor& right)
    {
        return BlockedMatMul(left, right, ExplainScalarMatMulPlan(left, right));
    }

} // namespace us4::runtime::backends::cpu_avx
