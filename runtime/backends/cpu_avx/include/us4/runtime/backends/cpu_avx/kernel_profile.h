#pragma once

#include <cstddef>
#include <string>

namespace us4::runtime::backends::cpu_avx
{

    enum class CpuInstructionSetLevel
    {
        kScalar,
        kAvx2,
        kAvx512,
        kAmx,
    };

    struct CpuVectorCapabilities
    {
        bool avx2 = false;
        bool avx512f = false;
        bool avx512Vnni = false;
        bool amxInt8 = false;
        bool amxBf16 = false;
        bool f16c = false;
        std::size_t l1DataBytes = 48U * 1024U;
        std::size_t l2BytesPerCore = 2048U * 1024U;
        std::size_t l3BytesShared = 32U * 1024U * 1024U;
        std::size_t hardwareThreadCount = 1U;
    };

    struct MicroKernelShape
    {
        std::size_t rows = 1;
        std::size_t cols = 1;
        std::size_t depth = 1;
        std::size_t laneWidth = 1;
    };

    struct CpuKernelProfile
    {
        CpuInstructionSetLevel level = CpuInstructionSetLevel::kScalar;
        std::string kernelTag = "scalar";
        std::size_t vectorBytes = 4;
        bool prefersOneDnnBlockGemm = false;
        bool supportsBitNetPopcount = false;
        bool supportsTernaryShuffleLut = false;
        bool supportsAmxTileConfig = false;
        MicroKernelShape matMul;
        MicroKernelShape attention;
    };

    [[nodiscard]] CpuInstructionSetLevel
    DetectInstructionSetLevel(const CpuVectorCapabilities& capabilities) noexcept;
    [[nodiscard]] CpuKernelProfile
    BuildKernelProfile(const CpuVectorCapabilities& capabilities) noexcept;
    [[nodiscard]] CpuVectorCapabilities DefaultReferenceCapabilities() noexcept;

} // namespace us4::runtime::backends::cpu_avx
