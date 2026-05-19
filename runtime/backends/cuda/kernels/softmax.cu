namespace us4::runtime::backends::cuda::kernels
{

    template <typename Quantizer>
    us4::core::Tensor SoftmaxHostKernel(const us4::core::Tensor& input, Quantizer&& quantize)
    {
        const std::size_t rows = input.Dim(0);
        const std::size_t cols = input.Dim(1);

        us4::core::Tensor output({rows, cols}, input.DataType());
        output.Fill(0.0F);

        for (std::size_t row = 0; row < rows; ++row)
        {
            float maxValue = input.At({row, 0U});
            for (std::size_t col = 1U; col < cols; ++col)
            {
                maxValue = std::max(maxValue, input.At({row, col}));
            }

            float normalizer = 0.0F;
            for (std::size_t col = 0U; col < cols; ++col)
            {
                const float exponent = std::exp(quantize(input.At({row, col}) - maxValue));
                output.At({row, col}) = exponent;
                normalizer += exponent;
            }

            for (std::size_t col = 0U; col < cols; ++col)
            {
                output.At({row, col}) =
                    normalizer > 0.0F ? quantize(output.At({row, col}) / normalizer) : 0.0F;
            }
        }

        return output;
    }

} // namespace us4::runtime::backends::cuda::kernels
