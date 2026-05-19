namespace us4::runtime::backends::cuda::kernels
{

    template <typename Quantizer>
    us4::core::Tensor MatMulHostKernel(const us4::core::Tensor& left,
                                       const us4::core::Tensor& right, Quantizer&& quantize)
    {
        const std::size_t rows = left.Dim(0);
        const std::size_t inner = left.Dim(1);
        const std::size_t cols = right.Dim(1);

        us4::core::Tensor output({rows, cols}, left.DataType());
        output.Fill(0.0F);

        for (std::size_t row = 0; row < rows; ++row)
        {
            for (std::size_t col = 0; col < cols; ++col)
            {
                float acc = 0.0F;
                for (std::size_t depth = 0; depth < inner; ++depth)
                {
                    const float lhs = quantize(left.At({row, depth}));
                    const float rhs = quantize(right.At({depth, col}));
                    acc += lhs * rhs;
                }
                output.At({row, col}) = quantize(acc);
            }
        }

        return output;
    }

} // namespace us4::runtime::backends::cuda::kernels
