#pragma once

#include <cstddef>
#include <string>

namespace us4::runtime::backends::onednn
{

    enum class OneDnnDataType
    {
        kFloat32,
        kBFloat16,
        kInt8,
        kInt4,
    };

    enum class OneDnnMemoryLayout
    {
        kRowMajor,
        kColumnMajor,
        kBlockedNC,
        kBlockedKN,
    };

    struct OneDnnMatMulDescriptor
    {
        std::size_t rows = 0;
        std::size_t inner = 0;
        std::size_t cols = 0;
        OneDnnDataType leftType = OneDnnDataType::kFloat32;
        OneDnnDataType rightType = OneDnnDataType::kFloat32;
        OneDnnDataType outputType = OneDnnDataType::kFloat32;
        bool transposeRight = false;
        bool fuseBias = false;
        bool fuseGelu = false;
        bool prepackWeights = true;
    };

    struct OneDnnMatMulPreference
    {
        bool allowAmx = false;
        bool allowAvx512 = false;
        bool preferPersistentCache = true;
        std::size_t l2BytesPerCore = 2048U * 1024U;
        std::size_t threadCountHint = 1U;
    };

    struct OneDnnMatMulPlan
    {
        bool valid = false;
        std::string reason;
        OneDnnMemoryLayout leftLayout = OneDnnMemoryLayout::kRowMajor;
        OneDnnMemoryLayout rightLayout = OneDnnMemoryLayout::kBlockedKN;
        OneDnnMemoryLayout outputLayout = OneDnnMemoryLayout::kRowMajor;
        std::size_t rowBlock = 0;
        std::size_t colBlock = 0;
        std::size_t depthBlock = 0;
        std::size_t scratchBytes = 0;
        bool useBrgemm = false;
        bool useAmxTiles = false;
        bool packWeights = false;
        std::string primitiveTag;
    };

    [[nodiscard]] OneDnnMatMulPlan
    BuildOneDnnMatMulPlan(const OneDnnMatMulDescriptor& descriptor,
                          const OneDnnMatMulPreference& preference) noexcept;

} // namespace us4::runtime::backends::onednn
