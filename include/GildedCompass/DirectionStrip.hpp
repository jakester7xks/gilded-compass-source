#pragma once
#include <cmath>
#include <optional>

namespace gc {
// Half-degree steps, five UI pixels per degree, 120-degree visible window.
// Quantization is circular: 359.9 degrees rounds to the same state as zero.
inline std::optional<unsigned> DirectionStripStep(double heading) noexcept {
    if (!std::isfinite(heading)) return std::nullopt;
    const double wrapped = std::fmod(std::fmod(heading, 360.0) + 360.0, 360.0);
    return static_cast<unsigned>(std::lround(wrapped * 2.0)) % 720;
}
}
