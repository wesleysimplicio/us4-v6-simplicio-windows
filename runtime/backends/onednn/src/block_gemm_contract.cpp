#include "us4/runtime/backends/onednn/block_gemm_contract.h"

#include <algorithm>

namespace us4::runtime::backends::onednn
{

    OneDnnMatMulPlan BuildOneDnnMatMulPlan(const OneDnnMatMulDescriptor& descriptor,
                                           const OneDnnMatMulPreference& preference) noexcept
    {
        OneDnnMatMulPlan plan;
        if (descriptor.rows == 0U || descriptor.inner == 0U || descriptor.cols == 0U)
        {
            plan.valid = false;
            plan.reason = "oneDNN matmul requires non-zero M, K, and N dimensions.";
            return plan;
        }

        plan.valid = true;
        plan.leftLayout = OneDnnMemoryLayout::kRowMajor;
        plan.rightLayout = descriptor.prepackWeights ? OneDnnMemoryLayout::kBlockedKN
                                                     : OneDnnMemoryLayout::kColumnMajor;
        plan.outputLayout = OneDnnMemoryLayout::kRowMajor;
        plan.packWeights = descriptor.prepackWeights;
        plan.useBrgemm = descriptor.rows >= 16U && descriptor.cols >= 16U;
        plan.useAmxTiles =
            preference.allowAmx && (descriptor.rightType == OneDnnDataType::kInt8 ||
                                    descriptor.rightType == OneDnnDataType::kBFloat16);

        if (plan.useAmxTiles)
        {
            plan.primitiveTag = "brgemm-amx";
            plan.rowBlock = 32U;
            plan.colBlock = 64U;
            plan.depthBlock = descriptor.rightType == OneDnnDataType::kInt8 ? 256U : 128U;
        }
        else if (preference.allowAvx512)
        {
            plan.primitiveTag = "brgemm-avx512";
            plan.rowBlock = 16U;
            plan.colBlock = 48U;
            plan.depthBlock = 128U;
        }
        else
        {
            plan.primitiveTag = "gemm-avx2-friendly";
            plan.rowBlock = 8U;
            plan.colBlock = 32U;
            plan.depthBlock = 96U;
        }

        if (descriptor.cols < plan.colBlock)
        {
            plan.colBlock = std::max<std::size_t>(descriptor.cols, 8U);
        }
        if (descriptor.rows < plan.rowBlock)
        {
            plan.rowBlock = std::max<std::size_t>(descriptor.rows, 4U);
        }
        if (descriptor.inner < plan.depthBlock)
        {
            plan.depthBlock = std::max<std::size_t>(descriptor.inner, 16U);
        }

        const std::size_t packedWeightBytes =
            descriptor.prepackWeights ? (plan.depthBlock * plan.colBlock * sizeof(float)) : 0U;
        const std::size_t accumulatorBytes = plan.rowBlock * plan.colBlock * sizeof(float);
        plan.scratchBytes =
            preference.preferPersistentCache
                ? packedWeightBytes + accumulatorBytes +
                      std::min<std::size_t>(preference.l2BytesPerCore / 4U, 512U * 1024U)
                : packedWeightBytes + accumulatorBytes;
        plan.reason = "Reference oneDNN-compatible blocking plan generated without runtime "
                      "dependency on oneDNN.";
        return plan;
    }

} // namespace us4::runtime::backends::onednn
