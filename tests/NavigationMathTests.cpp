#include "GildedCompass/NavigationMath.hpp"
#include <cmath>
#include <iostream>
#include <limits>
#include <numbers>

int main() {
    int failures = 0;
    const auto check = [&](bool ok, const char* message) {
        if (!ok) { std::cerr << message << '\n'; ++failures; }
    };
    const auto near = [](std::optional<double> value, double expected) {
        return value && std::abs(*value - expected) < 1e-8;
    };
    const double half = std::sqrt(0.5);
    check(near(gc::CameraYawRadians({0,0,0,1}), 0), "+Z faces zero mathematical yaw");
    check(near(gc::CameraYawRadians({0,half,0,half}), std::numbers::pi / 2), "+X faces positive half-pi yaw");
    check(near(gc::CameraYawRadians({0,-half,0,half}), -std::numbers::pi / 2), "-X faces negative half-pi yaw");
    check(near(gc::CameraYawRadians({0,-half,0,-half}), std::numbers::pi / 2), "q and -q represent the same rotation");
    check(near(gc::CameraYawRadians({0,half*1.02,0,half*1.02}), std::numbers::pi / 2), "Accepted norm drift is normalized");
    check(!gc::CameraYawRadians({0,0,0,0}), "Zero quaternion must be rejected");
    check(!gc::CameraYawRadians({0,0,0,2}), "Non-unit quaternion must be rejected");
    check(!gc::CameraYawRadians({std::numeric_limits<double>::quiet_NaN(),0,0,1}), "NaN must be rejected");
    check(!gc::CameraYawRadians({0,0,0,std::numeric_limits<double>::infinity()}), "Infinity must be rejected");
    check(!gc::CameraYawRadians({half,0,0,half}), "Vertical view has no stable planar yaw");
    // Pitch and yaw together: a 30-degree pitch must preserve a 90-degree yaw.
    const double pitch = std::numbers::pi / 12;
    check(near(gc::CameraYawRadians({half*std::sin(pitch),half*std::cos(pitch),
        -half*std::sin(pitch),half*std::cos(pitch)}), std::numbers::pi / 2),
        "Planar yaw survives camera pitch");
    return failures ? 1 : 0;
}
