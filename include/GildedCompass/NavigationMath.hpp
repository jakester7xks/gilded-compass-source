#pragma once
#include <optional>

namespace gc {
struct Quaternion {
    double x, y, z, w;
};

// Pure math on an explicitly supplied sample, not a game-memory reader.
// Returns yaw in radians, measured from +Z toward +X, in [-pi, pi].
// This is NOT a compass heading until world north and sign are calibrated.
// Rejects invalid rotations and near-vertical views with undefined planar yaw.
[[nodiscard]] std::optional<double> CameraYawRadians(Quaternion rotation);
}
