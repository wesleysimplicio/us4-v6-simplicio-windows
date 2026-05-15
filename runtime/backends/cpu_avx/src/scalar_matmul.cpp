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

    us4::core::Tensor ScalarMatMul(const us4::core::Tensor& left, const us4::core::Tensor& right)
    {
        const MatMulValidation validation = ValidateScalarMatMul(left, right);
        if (!validation.ok)
        {
            throw std::invalid_argument(validation.message);
        }

        const std::size_t rows = left.Dim(0);
        const std::size_t inner = left.Dim(1);
        const std::size_t cols = right.Dim(1);

        us4::core::Tensor output({rows, cols});
        output.Fill(0.0F);

        for (std::size_t row = 0; row < rows; ++row)
        {
            for (std::size_t k = 0; k < inner; ++k)
            {
                const float leftValue = left.At({row, k});
                for (std::size_t col = 0; col < cols; ++col)
                {
                    output.At({row, col}) += leftValue * right.At({k, col});
                }
            }
        }

        return output;
    }

} // namespace us4::runtime::backends::cpu_avx
