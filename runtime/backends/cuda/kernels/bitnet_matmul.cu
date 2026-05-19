namespace us4::runtime::backends::cuda::kernels
{

    [[nodiscard]] float BitNetMatMulHostKernel(const std::vector<float>& activations,
                                               const CudaBitNetPackedRow& packed)
    {
        float accumulator = 0.0F;
        for (std::size_t index = 0U; index < packed.elementCount; ++index)
        {
            const std::uint8_t mask = static_cast<std::uint8_t>(1U << (index % 8U));
            const bool positive = (packed.positiveBits[index / 8U] & mask) != 0U;
            const bool negative = (packed.negativeBits[index / 8U] & mask) != 0U;

            if (positive)
            {
                accumulator += activations[index];
            }
            else if (negative)
            {
                accumulator -= activations[index];
            }
        }

        return accumulator;
    }

} // namespace us4::runtime::backends::cuda::kernels
