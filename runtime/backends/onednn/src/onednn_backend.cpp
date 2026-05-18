#include "us4/runtime/backends/onednn/onednn_backend.h"

#include <stdexcept>

namespace us4::runtime::backends::onednn
{

    OneDnnBackend::OneDnnBackend(OneDnnBackendOptions options) noexcept : options_(options) {}

    OneDnnMatMulPlan OneDnnBackend::PlanMatMul(const us4::core::Tensor& left,
                                               const us4::core::Tensor& right,
                                               const cpu_avx::CpuKernelProfile* profile) const
    {
        if (left.Rank() != 2U || right.Rank() != 2U || left.Dim(1) != right.Dim(0))
        {
            OneDnnMatMulPlan plan;
            plan.valid = false;
            plan.reason = "OneDnnBackend requires [M, K] x [K, N] tensors.";
            return plan;
        }

        const cpu_avx::CpuKernelProfile fallbackProfile =
            cpu_avx::BuildKernelProfile(cpu_avx::DefaultReferenceCapabilities());
        const cpu_avx::CpuKernelProfile& selectedProfile =
            profile != nullptr ? *profile : fallbackProfile;

        return BuildOneDnnMatMulPlan(
            {
                .rows = left.Dim(0),
                .inner = left.Dim(1),
                .cols = right.Dim(1),
                .leftType = OneDnnDataType::kFloat32,
                .rightType = selectedProfile.level == cpu_avx::CpuInstructionSetLevel::kAmx
                                 ? OneDnnDataType::kBFloat16
                                 : OneDnnDataType::kFloat32,
                .outputType = OneDnnDataType::kFloat32,
                .prepackWeights = true,
            },
            {
                .allowAmx = options_.allowAmx &&
                            selectedProfile.level == cpu_avx::CpuInstructionSetLevel::kAmx,
                .allowAvx512 = options_.allowAvx512 &&
                               (selectedProfile.level == cpu_avx::CpuInstructionSetLevel::kAvx512 ||
                                selectedProfile.level == cpu_avx::CpuInstructionSetLevel::kAmx),
                .preferPersistentCache = options_.preferPersistentCache,
                .l2BytesPerCore = options_.l2BytesPerCore,
                .threadCountHint = options_.threadCountHint,
            });
    }

    OneDnnMatMulResult OneDnnBackend::ExecuteMatMul(const us4::core::Tensor& left,
                                                    const us4::core::Tensor& right,
                                                    const cpu_avx::CpuKernelProfile* profile) const
    {
        OneDnnMatMulResult result;
        result.plan = PlanMatMul(left, right, profile);
        if (!result.plan.valid)
        {
            throw std::invalid_argument(result.plan.reason);
        }

        const cpu_avx::CpuKernelProfile fallbackProfile =
            cpu_avx::BuildKernelProfile(cpu_avx::DefaultReferenceCapabilities());
        const cpu_avx::CpuKernelProfile& selectedProfile =
            profile != nullptr ? *profile : fallbackProfile;

        result.blockedPlan = cpu_avx::MakeReferenceMatMulPlan(left.Dim(0), left.Dim(1),
                                                              right.Dim(1), &selectedProfile);
        result.blockedPlan.tile.rowBlock =
            std::max(result.blockedPlan.tile.rowBlock, result.plan.rowBlock);
        result.blockedPlan.tile.colBlock =
            std::max(result.blockedPlan.tile.colBlock, result.plan.colBlock);
        result.blockedPlan.tile.depthBlock =
            std::max(result.blockedPlan.tile.depthBlock, result.plan.depthBlock);
        result.blockedPlan.packRightHandSide = result.plan.packWeights;
        result.blockedPlan.useOneDnnFriendlyBlocking = true;
        result.blockedPlan.kernelTag = "onednn-" + result.plan.primitiveTag;
        result.output = cpu_avx::BlockedMatMul(left, right, result.blockedPlan);
        return result;
    }

} // namespace us4::runtime::backends::onednn
