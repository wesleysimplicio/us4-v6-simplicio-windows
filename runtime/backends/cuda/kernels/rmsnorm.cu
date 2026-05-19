namespace us4::runtime::backends::cuda::kernels
{

    template <typename Quantizer>
    us4::core::Tensor RmsNormHostKernel(const us4::core::Tensor& input,
                                        const us4::core::Tensor& gamma, const float epsilon,
                                        Quantizer&& quantize)
    {
        const std::size_t rows = input.Dim(0);
        const std::size_t cols = input.Dim(1);

        us4::core::Tensor output({rows, cols}, input.DataType());
        output.Fill(0.0F);

        for (std::size_t row = 0; row < rows; ++row)
        {
            float meanSquare = 0.0F;
            for (std::size_t col = 0; col < cols; ++col)
            {
                const float value = input.At({row, col});
                meanSquare += value * value;
            }
            meanSquare /= static_cast<float>(cols);
            const float inverseRms = 1.0F / std::sqrt(meanSquare + epsilon);

            for (std::size_t col = 0; col < cols; ++col)
            {
                const float normalized = input.At({row, col}) * inverseRms;
                output.At({row, col}) = quantize(normalized * gamma.At({0U, col}));
            }
        }

        return output;
    }

} // namespace us4::runtime::backends::cuda::kernels
