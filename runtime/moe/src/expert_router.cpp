#include "us4/runtime/moe/expert_router.h"

#include <cmath>

namespace us4::runtime::moe
{

    std::vector<ExpertRoute> ExpertRouter::BuildPlan(const std::string& tokenWindow,
                                                     const RoutingPolicy& policy) const
    {
        std::vector<ExpertRoute> routes;
        routes.reserve(policy.topK);
        const float tokenBias = tokenWindow.empty()
                                    ? 0.0F
                                    : static_cast<float>((tokenWindow.size() % 17U) + 1U) * 0.05F;
        for (std::size_t index = 0; index < policy.topK; ++index)
        {
            routes.push_back(ExpertRoute{
                .expertId = index,
                .score = tokenBias + (1.0F / static_cast<float>(index + 1U)),
                .placement =
                    policy.allowCrossDeviceExperts && (index % 2U == 1U) ? "host" : "device",
            });
        }
        return routes;
    }

    std::vector<ExpertRoute> ExpertRouter::SelectTopK(const std::string& tokenWindow,
                                                      const std::size_t k) const
    {
        return BuildPlan(tokenWindow, RoutingPolicy{.topK = k});
    }

    RoutingStats ExpertRouter::Evaluate(const std::vector<ExpertRoute>& routes) const
    {
        RoutingStats stats{};
        if (routes.empty())
        {
            return stats;
        }

        float totalWeight = 0.0F;
        for (const auto& route : routes)
        {
            totalWeight += std::exp(route.score);
            if (route.placement == "host")
            {
                ++stats.hostRouteCount;
            }
            else
            {
                ++stats.deviceRouteCount;
            }
        }

        const float uniformWeight = 1.0F / static_cast<float>(routes.size());
        for (const auto& route : routes)
        {
            const float normalized = std::exp(route.score) / totalWeight;
            stats.entropy -= normalized * std::log(std::max(normalized, 1e-6F));
            const float delta = normalized - uniformWeight;
            stats.loadBalanceLoss += delta * delta;
        }

        return stats;
    }

} // namespace us4::runtime::moe
