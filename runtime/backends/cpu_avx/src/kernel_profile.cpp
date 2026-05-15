#include "us4/runtime/backends/cpu_avx/kernel_profile.h"

namespace us4::runtime::backends::cpu_avx
{

    CpuInstructionSetLevel
    DetectInstructionSetLevel(const CpuVectorCapabilities& capabilities) noexcept
    {
        if (capabilities.amxInt8 || capabilities.amxBf16)
        {
            return CpuInstructionSetLevel::kAmx;
        }

        if (capabilities.avx512f || capabilities.avx512Vnni)
        {
            return CpuInstructionSetLevel::kAvx512;
        }

        if (capabilities.avx2)
        {
            return CpuInstructionSetLevel::kAvx2;
        }

        return CpuInstructionSetLevel::kScalar;
    }

    CpuKernelProfile BuildKernelProfile(const CpuVectorCapabilities& capabilities) noexcept
    {
        CpuKernelProfile profile;
        profile.level = DetectInstructionSetLevel(capabilities);

        switch (profile.level)
        {
        case CpuInstructionSetLevel::kAmx:
            profile.kernelTag = capabilities.amxBf16 ? "amx-bf16-blocked" : "amx-int8-blocked";
            profile.vectorBytes = 64;
            profile.prefersOneDnnBlockGemm = true;
            profile.supportsBitNetPopcount = true;
            profile.supportsTernaryShuffleLut = true;
            profile.supportsAmxTileConfig = true;
            profile.matMul = MicroKernelShape{32, 64, 128, 16};
            profile.attention = MicroKernelShape{16, 64, 64, 16};
            break;
        case CpuInstructionSetLevel::kAvx512:
            profile.kernelTag = capabilities.avx512Vnni ? "avx512-vnni-blocked" : "avx512-blocked";
            profile.vectorBytes = 64;
            profile.prefersOneDnnBlockGemm = true;
            profile.supportsBitNetPopcount = true;
            profile.supportsTernaryShuffleLut = true;
            profile.matMul = MicroKernelShape{16, 32, 128, 16};
            profile.attention = MicroKernelShape{8, 32, 64, 16};
            break;
        case CpuInstructionSetLevel::kAvx2:
            profile.kernelTag = "avx2-blocked";
            profile.vectorBytes = 32;
            profile.prefersOneDnnBlockGemm = capabilities.l2BytesPerCore >= (1024U * 1024U) &&
                                             capabilities.hardwareThreadCount >= 4U;
            profile.supportsBitNetPopcount = true;
            profile.supportsTernaryShuffleLut = true;
            profile.matMul = MicroKernelShape{8, 16, 96, 8};
            profile.attention = MicroKernelShape{4, 16, 48, 8};
            break;
        case CpuInstructionSetLevel::kScalar:
        default:
            profile.kernelTag = "scalar";
            profile.vectorBytes = 4;
            profile.prefersOneDnnBlockGemm = false;
            profile.supportsBitNetPopcount = false;
            profile.supportsTernaryShuffleLut = false;
            profile.supportsAmxTileConfig = false;
            profile.matMul = MicroKernelShape{4, 8, 32, 1};
            profile.attention = MicroKernelShape{2, 8, 16, 1};
            break;
        }

        return profile;
    }

    CpuVectorCapabilities DefaultReferenceCapabilities() noexcept
    {
        CpuVectorCapabilities capabilities;
        capabilities.avx2 = true;
        capabilities.f16c = true;
        capabilities.hardwareThreadCount = 8U;
        capabilities.l2BytesPerCore = 1280U * 1024U;
        capabilities.l3BytesShared = 16U * 1024U * 1024U;
        return capabilities;
    }

} // namespace us4::runtime::backends::cpu_avx
