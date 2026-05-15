#include "us4/runtime/moe/expert_router.h"

namespace us4::runtime::moe
{

    std::vector<ExpertRoute> ExpertRouter::BuildPlan(const std::string& tokenWindow,
                                                     const RoutingPolicy& policy) const
    {
        std::vector<ExpertRoute> routes;
        routes.reserve(policy.topK);
        for (std::size_t index = 0; index < policy.topK; ++index)
        {
            routes.push_back(ExpertRoute{
                .expertId = index,
                .score = tokenWindow.empty() ? 0.0F : (1.0F / static_cast<float>(index + 1)),
                .placement =
                    policy.allowCrossDeviceExperts && (index % 2U == 1U) ? "host" : "device",
            });
        }
        return routes;
    }

    std::vector<ExpertRoute> ExpertRouter::SelectTopK(const std::string& tokenWindow,
                                                      std::size_t k) const
    {
        return BuildPlan(tokenWindow, RoutingPolicy{.topK = k});
    }

} // namespace us4::runtime::moe
