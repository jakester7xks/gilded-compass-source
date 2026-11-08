#pragma once
#include <cstdint>
#include <optional>

namespace gc {
struct HeadingSample {
    std::optional<double> degrees;
    std::uint64_t at = 0;
    bool active = false;
};
struct SelectedHeading {
    std::optional<double> degrees;
    bool aiming = false;
    bool action = false;
};
inline SelectedHeading SelectHeading(const HeadingSample& tps, const HeadingSample& aim,
                                    std::uint64_t now, const HeadingSample& action = {}) {
    // Aim wins while its output is being refreshed and its native aim flag is
    // set. Invalid aim data must not silently use a dormant TPS orientation.
    if (aim.active && now >= aim.at && now - aim.at <= 100)
        return {aim.degrees, true};
    // Output callbacks also serve non-bow actions. Use those only when TPS
    // has stopped refreshing; an explicit bow flag is not required for them.
    const bool freshTps = tps.active && now >= tps.at && now - tps.at <= 100;
    if (!freshTps && action.active && now >= action.at && now - action.at <= 100)
        return {action.degrees, false, true};
    if (tps.active && now >= tps.at && now - tps.at <= 500)
        return {tps.degrees, false};
    return {};
}
}
