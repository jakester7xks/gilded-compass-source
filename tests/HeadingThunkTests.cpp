#include <Windows.h>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <initializer_list>

using Forward = uint64_t(*)(uintptr_t, double, uint64_t, float, double, uint64_t, double, float);
static unsigned observed = 0;
static uint64_t Original(uintptr_t a, double b, uint64_t c, float d,
                         double e, uint64_t f, double g, float h) {
    return a == 0x12345678 && b == 2.25 && c == 0xfedcba9876543210ull && d == 4.5f
        && e == 5.125 && f == 0x87654321 && g == 7.75 && h == 8.875f ? 0xabcddcba12344321ull : 0;
}
extern "C" {
void* gc_headingCameraOriginal = reinterpret_cast<void*>(&Original);
void* gc_headingTickOriginal = reinterpret_cast<void*>(&Original);
void* gc_headingAimOriginal = reinterpret_cast<void*>(&Original);
void* gc_headingTwoTargetOriginal = reinterpret_cast<void*>(&Original);
uint64_t gc_HeadingCameraThunk(uintptr_t, double, uint64_t, float, double, uint64_t, double, float);
uint64_t gc_HeadingTickThunk(uintptr_t, double, uint64_t, float, double, uint64_t, double, float);
uint64_t gc_HeadingAimThunk(uintptr_t, double, uint64_t, float, double, uint64_t, double, float);
uint64_t gc_HeadingActionThunk(uintptr_t, double, uint64_t, float, double, uint64_t, double, float);
uint64_t gc_HeadingTwoTargetThunk(uintptr_t, double, uint64_t, float, double, uint64_t, double, float);
void gc_HeadingCamera(uintptr_t self) {
    if (self == 0x12345678) ++observed;
    // Exercise a real call path that may clobber caller-saved registers.
    volatile double work = std::sin(static_cast<double>(GetTickCount64()));
    (void)work;
}
void gc_HeadingTick(uintptr_t self) { gc_HeadingCamera(self); }
void gc_HeadingAim(uintptr_t self) { gc_HeadingCamera(self); }
void gc_HeadingAction(uintptr_t self) { gc_HeadingCamera(self); }
}
int main() {
    for (Forward thunk : {&gc_HeadingCameraThunk, &gc_HeadingTickThunk, &gc_HeadingAimThunk,
                          &gc_HeadingActionThunk, &gc_HeadingTwoTargetThunk}) {
        for (unsigned i = 0; i < 10000; ++i)
            if (thunk(0x12345678, 2.25, 0xfedcba9876543210ull, 4.5f, 5.125,
                      0x87654321, 7.75, 8.875f) != 0xabcddcba12344321ull) return 1;
    }
    if (observed != 50000) return 2;
    puts("Native observer thunks preserve mixed register/stack arguments and original return values.");
    return 0;
}
