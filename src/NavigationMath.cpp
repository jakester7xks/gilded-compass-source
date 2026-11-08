// Camera quaternion/forward-vector conversion adapted from Glint Spotter:
// Copyright (c) 2026 Seth. MIT License.
// See third_party/licenses/glint-spotter.LICENSE and docs/REUSE_REVIEW.md.
// Pinned upstream: 9bd3e8231b468591dbeae7f340526a8cb497650b,
// mod/src/game/camera.cpp, gs::camera::Read.
#include "GildedCompass/NavigationMath.hpp"
#include <cmath>

namespace gc {
std::optional<double> CameraYawRadians(Quaternion rotation) {
    const double normSquared = rotation.x * rotation.x + rotation.y * rotation.y
        + rotation.z * rotation.z + rotation.w * rotation.w;
    if (!std::isfinite(normSquared) || normSquared < 0.9 || normSquared > 1.1)
        return std::nullopt;

    // Normalize accepted drift before deriving the forward vector. No game
    // object layout, signatures, memory access or hooks are imported here.
    const double norm = std::sqrt(normSquared);
    const double x = rotation.x / norm, y = rotation.y / norm;
    const double z = rotation.z / norm, w = rotation.w / norm;
    const double forwardX = 2.0 * (x * z + w * y);
    const double forwardZ = 1.0 - 2.0 * (x * x + y * y);
    if (std::hypot(forwardX, forwardZ) < 1e-6) return std::nullopt;
    return std::atan2(forwardX, forwardZ);
}
}
