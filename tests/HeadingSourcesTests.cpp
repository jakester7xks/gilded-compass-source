#include "GildedCompass/HeadingSources.hpp"
#include <cstdio>

int main() {
    const gc::HeadingSample tps{45, 1000, true};
    gc::HeadingSample aim{90, 1001, true};
    auto h = gc::SelectHeading(tps, aim, 1002);
    if (!h.aiming || h.degrees != 90) return 1; // Active aim wins over fresh TPS.
    aim.degrees = 120;
    if (gc::SelectHeading(tps, aim, 1003).degrees != 120) return 2; // Rotation follows.
    aim.degrees.reset();
    if (gc::SelectHeading(tps, aim, 1003).degrees) return 3; // Invalid aim isn't frozen TPS.
    aim = {120, 1001, false};
    h = gc::SelectHeading(tps, aim, 1003);
    if (h.aiming || h.degrees != 45) return 4; // Explicit exit returns immediately.
    aim.active = true;
    h = gc::SelectHeading({50, 1102, true}, aim, 1102);
    if (h.aiming || h.degrees != 50) return 5; // Stopped aim callback expires.
    if (gc::SelectHeading(tps, aim, 1501).degrees) return 6; // No indefinite stale heading.
    if (gc::SelectHeading(tps, aim, 999).degrees) return 7; // No future sample.
    gc::HeadingSample action{210, 1600, true};
    h = gc::SelectHeading(tps, {}, 1601, action);
    if (!h.action || h.degrees != 210) return 8; // Actions survive a fully expired TPS.
    action = {220, 1610, true};
    if (gc::SelectHeading(tps, {}, 1611, action).degrees != 220) return 9;
    h = gc::SelectHeading({80, 1611, true}, {}, 1612, action);
    if (h.action || h.degrees != 80) return 10; // Returning TPS wins over lingering action.
    h = gc::SelectHeading(tps, {90, 1611, true}, 1612, action);
    if (!h.aiming || h.degrees != 90) return 11; // Bow behavior remains authoritative.
    if (gc::SelectHeading(tps, {}, 1711, action).degrees) return 12; // Action expires too.
    action.degrees.reset();
    if (gc::SelectHeading({45, 1500, true}, {}, 1612, action).degrees) return 13;
    // An invalid active action doesn't turn into an old but still <=500ms TPS heading.
    puts("Aim/TPS/action priority, rotation, invalid samples, exit and expiration passed.");
}
