#include "GildedCompass/DirectionStrip.hpp"
#include <limits>
#include <cstdio>
int main() {
    if (gc::DirectionStripStep(0) != 0 || gc::DirectionStripStep(90) != 180
        || gc::DirectionStripStep(180) != 360 || gc::DirectionStripStep(270) != 540) return 1;
    if (gc::DirectionStripStep(359.9) != 0 || gc::DirectionStripStep(-0.1) != 0
        || gc::DirectionStripStep(360.5) != 1 || gc::DirectionStripStep(-90) != 540) return 2;
    if (gc::DirectionStripStep(std::numeric_limits<double>::quiet_NaN())
        || gc::DirectionStripStep(std::numeric_limits<double>::infinity())) return 3;
    // A visible landmark's x coordinate must move left when yaw increases,
    // including at the seam where the equivalent repeated landmark takes over.
    for (unsigned i = 0; i < 720; ++i) {
        const auto state = gc::DirectionStripStep(i * 0.5);
        if (!state || *state != i) return 4;
        const double pixel = 300.0 + 5.0 * (i * 0.5) - 2.5 * *state;
        if (pixel != 300.0) return 5;
    }
    puts("Direction strip cardinals, seam, invalid input and 720 center positions passed.");
    return 0;
}
