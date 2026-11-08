// Native compass bridge with opt-in experimental objective association. No D3D/DXGI.
// Native element traversal informed by Outfit Manager (MIT, Strahlimeier 2026).
#include <Windows.h>
#include <bcrypt.h>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <vector>
#include "GildedCompass/NavigationMath.hpp"
#include "GildedCompass/DirectionStrip.hpp"
#include "GildedCompass/ObjectiveAssociation.hpp"
#include "GildedCompass/HeadingSources.hpp"
#include "HeadingBuild.hpp" // Generated from the user's offline executable survey.
#include "QuestObservationBuild.hpp"

extern "C" {
void* gc_headingCameraOriginal = nullptr;
void* gc_headingTickOriginal = nullptr;
void* gc_headingAimOriginal = nullptr;
void* gc_headingTwoTargetOriginal = nullptr;
void gc_HeadingCameraThunk();
void gc_HeadingTickThunk();
void gc_HeadingAimThunk();
void gc_HeadingActionThunk();
void gc_HeadingTwoTargetThunk();
}
namespace {
HMODULE module;
uintptr_t base;
HANDLE logHandle = INVALID_HANDLE_VALUE;
std::once_flag once;
std::atomic<bool> faulted{false};
std::mutex poseMutex;
gc::HeadingSample tpsPose, aimPose, actionPose;
int actionSource = 0;
using SetText = void(__fastcall*)(uintptr_t, const char*, uintptr_t);
using Intern = void(__fastcall*)(uint64_t*, const char*, uint32_t, uint32_t);
SetText setText;
Intern intern;
using AddClass = bool(__fastcall*)(uintptr_t, const uint32_t*);
using RemoveClass = uint32_t(__fastcall*)(uintptr_t, const uint32_t*);
using Unary = void(__fastcall*)(uintptr_t);
AddClass classAdd;
RemoveClass classRemove;
Unary classSort, restyle;
uint32_t yawClasses[720]{};
uint32_t unavailableClass;
using GetText = void(__fastcall*)(uintptr_t, char*, int);
GetText getText;
bool observeQuests = false;
bool objectiveTest = false;
uint32_t objectiveClasses[602]{}; // 0..600 = screen position; 601 = hidden.

void Log(const char* text) noexcept {
    if (logHandle != INVALID_HANDLE_VALUE) {
        DWORD written;
        WriteFile(logHandle, text, static_cast<DWORD>(strlen(text)), &written, nullptr);
        FlushFileBuffers(logHandle);
    }
}
template<class T> bool Read(uintptr_t at, T& out) noexcept {
    __try { memcpy(&out, reinterpret_cast<const void*>(at), sizeof(T)); return true; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
uintptr_t Pointer(uintptr_t at) { uintptr_t p = 0; Read(at, p); return p; }

bool HashMatches(const wchar_t* filename) {
    std::ifstream input(filename, std::ios::binary);
    if (!input) return false;
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0) return false;
    DWORD size = 0, returned = 0;
    bool ok = BCryptGetProperty(algorithm, BCRYPT_OBJECT_LENGTH,
        reinterpret_cast<PUCHAR>(&size), sizeof(size), &returned, 0) >= 0;
    std::vector<unsigned char> object(size);
    unsigned char digest[32]{};
    if (ok) ok = BCryptCreateHash(algorithm, &hash, object.data(), size, nullptr, 0, 0) >= 0;
    char buffer[65536];
    while (ok && input) {
        input.read(buffer, sizeof(buffer));
        const auto count = input.gcount();
        if (count) ok = BCryptHashData(hash, reinterpret_cast<PUCHAR>(buffer), static_cast<ULONG>(count), 0) >= 0;
    }
    if (ok) ok = input.eof() && BCryptFinishHash(hash, digest, sizeof(digest), 0) >= 0;
    if (hash) BCryptDestroyHash(hash);
    BCryptCloseAlgorithmProvider(algorithm, 0);
    return ok && memcmp(digest, gc::native_build::sha256, 32) == 0;
}

// Only our uniquely classed label is eligible. No stored element pointers survive a tick.
uintptr_t FindLabel(uintptr_t element, uint32_t classId, unsigned depth, unsigned& budget) {
    if (!element || depth > 16 || budget == 0) return 0;
    --budget;
    const auto holder = Pointer(element + 0x20);
    if (!holder || Pointer(holder) != element) return 0;
    const auto classes = Pointer(holder + 8), data = Pointer(classes);
    uint32_t count = 0;
    if (!Read(classes + 8, count) || count > 64) return 0;
    for (uint32_t i = 0; data && i < count; ++i) {
        uint32_t id = 0;
        if (Read(data + i * 4, id) && id == classId) return element;
    }
    const auto children = Pointer(element + 0x40);
    if (!Read(element + 0x48, count) || count > 256) return 0;
    for (uint32_t i = 0; children && i < count; ++i)
        if (auto found = FindLabel(Pointer(children + i * 8), classId, depth + 1, budget)) return found;
    return 0;
}
bool Assign(uintptr_t element, const char* text) noexcept {
    __try { setText(element, text, 0); return true; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
bool LabelClass(uint32_t& value) noexcept {
    uint64_t id = 0;
    __try { intern(&id, "gc-heading-label", 1, 0x2ffff); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
    value = static_cast<uint32_t>(id);
    return value != 0;
}
bool ClassId(const char* name, uint32_t& value) noexcept {
    uint64_t id = 0;
    __try { intern(&id, name, 1, 0x2ffff); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
    value = static_cast<uint32_t>(id);
    return value != 0;
}
bool Text(uintptr_t element, char* output, int size) noexcept {
    if (!element || size < 1) return false;
    output[0] = '\0';
    __try { getText(element, output, size); }
    __except (EXCEPTION_EXECUTE_HANDLER) { output[0] = '\0'; return false; }
    output[size-1] = '\0';
    for (int i=0; output[i]; ++i)
        if (output[i] == '\r' || output[i] == '\n' || output[i] == '\t' || output[i] == '"') output[i] = ' ';
    return true;
}
unsigned CountClass(uintptr_t element, uint32_t id, unsigned depth, unsigned& budget) {
    if (!element || depth > 16 || !budget) return 0;
    --budget;
    const auto holder=Pointer(element+0x20), vec=Pointer(holder+8), data=Pointer(vec);
    if (!holder || Pointer(holder)!=element) return 0;
    uint32_t count=0;
    if (!Read(vec+8,count) || count>64) return 0;
    unsigned found=0;
    for (uint32_t i=0; data && i<count; ++i) {
        uint32_t value=0; if(Read(data+i*4,value) && value==id) found=1;
    }
    const auto children=Pointer(element+0x40);
    if (!Read(element+0x48,count) || count>256) return found;
    for (uint32_t i=0; children && i<count && budget; ++i)
        found+=CountClass(Pointer(children+i*8),id,depth+1,budget);
    return found;
}
// Diagnostic evidence only: raw state is never promoted to a tracked quest.
// Six minutes maximum, two seconds between snapshots, 64 icon rows per sample.
void ObserveQuestData(uintptr_t self, ULONGLONG now) noexcept {
    static ULONGLONG last=0;
    static unsigned samples=0;
    static bool stopped=false;
    if (!observeQuests || now-last<2000 || stopped) return;
    if(samples>=180) { stopped=true; Log("QUEST_OBSERVATION complete; compass continues normally.\r\n"); return; }
    last=now; ++samples;
    uint32_t titleClass=0,wrapClass=0,trackedClass=0,hiddenClass=0;
    if(!ClassId("cpp-stage-info-title",titleClass) || !ClassId("cpp-quest-info-wrap",wrapClass)
       || !ClassId("cpp-tracking",trackedClass) || !ClassId("cpp-none",hiddenClass)) {
        stopped=true; Log("QUEST_OBSERVATION stopped: class lookup failed.\r\n"); return;
    }
    const auto root=Pointer(self+8);
    unsigned budget=2048;
    const auto wrap=FindLabel(root,wrapClass,0,budget);
    budget=2048;
    const auto title=FindLabel(wrap,titleClass,0,budget);
    char titleText[256]{},distanceText[96]{};
    const bool titleRead=Text(title,titleText,sizeof(titleText));
    const bool distanceRead=Text(Pointer(self+0x448),distanceText,sizeof(distanceText));
    int distance=-1; Read(self+0x598,distance);
    budget=2048; const auto tracking=CountClass(wrap,trackedClass,0,budget);
    budget=2048; const auto hidden=CountClass(wrap,hiddenClass,0,budget);
    uint32_t count=0;
    const bool countRead=Read(base+gc::quest_observation::iconCount,count);
    const auto list=Pointer(base+gc::quest_observation::iconList);
    char output[24576];
    int used=sprintf_s(output,"QUEST_OBSERVATION sample=%u uptime_ms=%llu title_read=%d title=\"%s\" tracking_class_count=%u hidden_class_count=%u distance_read=%d distance_text=\"%s\" distance_cache=%d icon_count=%u\r\n",
        samples,now,titleRead,titleText,tracking,hidden,distanceRead,distanceText,distance,count);
    unsigned written=0,invalid=0;
    if(countRead && count<=4096 && list) {
        for(uint32_t i=0;i<count && written<64;++i) {
            const auto icon=Pointer(list+static_cast<uintptr_t>(i)*8);
            if(!icon || Pointer(icon)!=base+gc::quest_observation::iconVtable) { ++invalid; continue; }
            uint16_t type=0; float position[3]{}; unsigned char flags[16]{};
            if(!Read(icon+0xde,type) || !Read(icon+0xe0,position) || !Read(icon+0x268,flags)) { ++invalid; continue; }
            if(!std::isfinite(position[0]) || !std::isfinite(position[1]) || !std::isfinite(position[2])) { ++invalid; continue; }
            char flagText[33]{};
            for(unsigned j=0;j<16;++j) sprintf_s(flagText+j*2,3,"%02x",flags[j]);
            const int added=sprintf_s(output+used,sizeof(output)-used," icon_index=%u type_raw=%u position_raw=(%.3f,%.3f,%.3f) flags_268_277=%s\r\n",
                i,type,position[0],position[1],position[2],flagText);
            if(added<0) break;
            used+=added; ++written;
        }
    }
    sprintf_s(output+used,sizeof(output)-used," quest_icon_rows=%u skipped_rows=%u bounded_or_truncated=%d (raw candidates, not classified objectives)\r\n",
        written,invalid,!countRead || count>4096 || written+invalid<count);
    Log(output);
}
// Change only our direction-state classes, following the native class/restyle
// path used by Outfit Manager. No direct writes to element layout fields.
bool StripState(uintptr_t element, uint32_t wanted, const uint32_t* states=yawClasses,
                unsigned stateCount=720, uint32_t hidden=unavailableClass) noexcept {
    const auto holder = Pointer(element + 0x20), vec = Pointer(holder + 8);
    if (!holder || Pointer(holder) != element || !vec) return false;
    const auto pending = Pointer(element + 0xa0);
    if (pending && Pointer(pending + 8)) return true; // Retry after its pending restyle.
    const auto data = Pointer(vec);
    uint32_t count = 0, ids[64]{};
    if (!Read(vec + 8, count) || count > 64 || (count && !data)) return false;
    bool present = false, changed = false;
    for (uint32_t i = 0; i < count; ++i) {
        if (!Read(data + i * 4, ids[i])) return false;
        if (ids[i] == wanted) present = true;
    }
    __try {
        for (uint32_t i = 0; i < count; ++i) {
            const auto id = ids[i];
            if (id == wanted) continue;
            bool ours = id && id == hidden;
            for (unsigned j=0;j<stateCount;++j) if (states[j] && id == states[j]) { ours = true; break; }
            if (ours) { classRemove(vec, &id); changed = true; }
        }
        if (!present) { classAdd(vec, &wanted); changed = true; }
        if (changed) { classSort(vec); restyle(holder); }
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
    return true;
}
std::optional<gc::AssociatedObjective> ReadObjectiveAssociation() {
    uint32_t count=0;
    if(!Read(base+gc::quest_observation::iconCount,count)||count>4096)return std::nullopt;
    const auto list=Pointer(base+gc::quest_observation::iconList);
    if(!list)return std::nullopt;
    gc::ObservedMapIcon records[32]{};
    unsigned used=0;
    for(uint32_t i=0;i<count;++i) {
        const auto icon=Pointer(list+static_cast<uintptr_t>(i)*8);
        if(!icon||Pointer(icon)!=base+gc::quest_observation::iconVtable)continue;
        uint16_t type=0;
        if(!Read(icon+0xde,type))return std::nullopt;
        if(type!=0 && type!=3)continue;
        if(used==32)return std::nullopt;
        float position[3]{}; unsigned char selected=0,active=0;
        if(!Read(icon+0xe0,position)||!Read(icon+0x273,selected)||!Read(icon+0x274,active))return std::nullopt;
        records[used++]={type,position[0],position[1],position[2],selected,active};
    }
    uint32_t after=0;
    if(!Read(base+gc::quest_observation::iconCount,after)||after!=count
        ||Pointer(base+gc::quest_observation::iconList)!=list)return std::nullopt;
    return gc::AssociateObjective(std::span(records,used));
}
void UpdateObjectiveTest(uintptr_t root, std::optional<double> heading) {
    if(!objectiveTest)return;
    uint32_t markerClass=0,distanceClass=0;
    if(!ClassId("gc-objective-marker",markerClass)||!ClassId("gc-objective-distance",distanceClass))return;
    unsigned budget=2048;
    const auto marker=FindLabel(root,markerClass,0,budget);
    if(!marker)return;
    budget=64;
    const auto distanceLabel=FindLabel(marker,distanceClass,0,budget);
    if(!distanceLabel)return;
    const auto associated=heading?ReadObjectiveAssociation():std::nullopt;
    const auto position=associated?gc::ObjectiveScreenPosition(associated->bearingDegrees,*heading):std::nullopt;
    const unsigned state=position?*position:601;
    if(!objectiveClasses[state]) {
        char name[40];
        if(position)sprintf_s(name,"gc-objective-x-%03u",state);
        else strcpy_s(name,"gc-objective-hidden");
        if(!ClassId(name,objectiveClasses[state]))return;
    }
    if(!StripState(marker,objectiveClasses[state],objectiveClasses,602,0)) {
        faulted=true; Log("STOPPED: objective test class update failed.\r\n"); return;
    }
    char text[40]{};
    if(associated && position)sprintf_s(text,"%um",associated->distanceMetres);
    if(!Assign(distanceLabel,text)) { faulted=true; Log("STOPPED: objective distance update failed.\r\n"); return; }
    static int loggedState=-1;
    const int current=associated?(position?2:1):0;
    if(loggedState!=current) {
        loggedState=current;
        Log(current==2?"DESTINATION_TEST: selected navigation destination visible (quest or manual pin).\r\n":
            current==1?"DESTINATION_TEST: selected destination outside compass view.\r\n":
                       "DESTINATION_TEST: no unambiguous selected destination; marker hidden.\r\n");
    }
}
bool Swap(uintptr_t slotRva, void* expected, void* replacement) {
    auto slot = reinterpret_cast<void**>(base + slotRva);
    DWORD old;
    if (!VirtualProtect(slot, sizeof(void*), PAGE_READWRITE, &old)) return false;
    const bool changed = InterlockedCompareExchangePointer(slot, replacement, expected) == expected;
    DWORD ignored;
    VirtualProtect(slot, sizeof(void*), old, &ignored);
    return changed;
}
void Initialize() {
    wchar_t dllPath[MAX_PATH]{}, exePath[MAX_PATH]{};
    if (!GetModuleFileNameW(module, dllPath, MAX_PATH) || !GetModuleFileNameW(nullptr, exePath, MAX_PATH)) return;
    auto ini = std::filesystem::path(dllPath); ini.replace_extension(L".ini");
    if (GetPrivateProfileIntW(L"HeadingTest", L"Enabled", 0, ini.c_str()) != 1) return;
    observeQuests = GetPrivateProfileIntW(L"HeadingTest", L"QuestDiagnostics", 0, ini.c_str()) == 1;
    objectiveTest = GetPrivateProfileIntW(L"HeadingTest", L"ObjectiveTest", 0, ini.c_str()) == 1;
    if (_wcsicmp(std::filesystem::path(exePath).filename().c_str(), L"CrimsonDesert.exe")) return;
    auto log = std::filesystem::path(dllPath); log.replace_extension(L".log");
    logHandle = CreateFileW(log.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    Log("Gilded Compass native bar. Objective association is experimental and opt-in.\r\n");
    if (!HashMatches(exePath)) { Log("REFUSED: executable hash differs from offline survey.\r\n"); return; }
    base = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    // Compare the loaded functions too: reject other mods' patches, including existing slot hooks.
    for (const auto& check : gc::native_build::checks) {
        unsigned char actual[16]{};
        if (!Read(base + check.rva, actual) || memcmp(actual, check.bytes, 16)) {
            Log("REFUSED: loaded function bytes differ.\r\n"); return;
        }
    }
    gc_headingCameraOriginal = reinterpret_cast<void*>(base + gc::native_build::cameraUpdate);
    gc_headingTickOriginal = reinterpret_cast<void*>(base + gc::native_build::minimapUpdate);
    gc_headingAimOriginal = reinterpret_cast<void*>(base + gc::native_build::aimOutput);
    gc_headingTwoTargetOriginal = reinterpret_cast<void*>(base + gc::native_build::twoTargetOutput);
    if (Pointer(base + gc::native_build::cameraSlot) != reinterpret_cast<uintptr_t>(gc_headingCameraOriginal)
        || Pointer(base + gc::native_build::minimapSlot) != reinterpret_cast<uintptr_t>(gc_headingTickOriginal)
        || Pointer(base + gc::native_build::aimSlot) != reinterpret_cast<uintptr_t>(gc_headingAimOriginal)) {
        Log("REFUSED: another mod already owns a required vtable slot.\r\n"); return;
    }
    setText = reinterpret_cast<SetText>(base + gc::native_build::setText);
    intern = reinterpret_cast<Intern>(base + gc::native_build::intern);
    getText = reinterpret_cast<GetText>(base + gc::native_build::getText);
    classAdd = reinterpret_cast<AddClass>(base + gc::native_build::classAdd);
    classRemove = reinterpret_cast<RemoveClass>(base + gc::native_build::classRemove);
    classSort = reinterpret_cast<Unary>(base + gc::native_build::classSort);
    restyle = reinterpret_cast<Unary>(base + gc::native_build::restyle);
    if (Pointer(base + gc::native_build::blackHoleSlot) != reinterpret_cast<uintptr_t>(gc_headingAimOriginal)
        || Pointer(base + gc::native_build::cinematicSlot) != reinterpret_cast<uintptr_t>(gc_headingAimOriginal)
        || Pointer(base + gc::native_build::twoTargetSlot) != reinterpret_cast<uintptr_t>(gc_headingTwoTargetOriginal)) {
        Log("REFUSED: another mod owns an action camera output slot.\r\n"); return;
    }
    HMODULE pinned;
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
        reinterpret_cast<LPCWSTR>(&gc_HeadingTickThunk), &pinned)) return;
    struct Hook { uintptr_t slot; void* original; void* thunk; };
    const Hook hooks[] = {
        {gc::native_build::cameraSlot, gc_headingCameraOriginal, reinterpret_cast<void*>(&gc_HeadingCameraThunk)},
        {gc::native_build::aimSlot, gc_headingAimOriginal, reinterpret_cast<void*>(&gc_HeadingAimThunk)},
        {gc::native_build::blackHoleSlot, gc_headingAimOriginal, reinterpret_cast<void*>(&gc_HeadingActionThunk)},
        {gc::native_build::cinematicSlot, gc_headingAimOriginal, reinterpret_cast<void*>(&gc_HeadingActionThunk)},
        {gc::native_build::twoTargetSlot, gc_headingTwoTargetOriginal, reinterpret_cast<void*>(&gc_HeadingTwoTargetThunk)},
        {gc::native_build::minimapSlot, gc_headingTickOriginal, reinterpret_cast<void*>(&gc_HeadingTickThunk)}
    };
    for (unsigned i = 0; i < sizeof(hooks)/sizeof(hooks[0]); ++i) {
        if (Swap(hooks[i].slot, hooks[i].original, hooks[i].thunk)) continue;
        while (i) { --i; Swap(hooks[i].slot, hooks[i].thunk, hooks[i].original); }
        Log("REFUSED: a required slot changed; earlier observations rolled back.\r\n"); return;
    }
    Log("Observers armed: TPS, Aim, BlackHole, CinematicLockOn and TwoTargetLockOn; waiting for custom compass.\r\n");
}
}

extern "C" void gc_HeadingCamera(uintptr_t self) noexcept {
    if (faulted.load()) return;
    static std::atomic<bool> said{false};
    if (!said.exchange(true)) Log("Camera update observed; using the yaw source confirmed in the numeric heading test.\r\n");
    float q[4]{};
    const bool read = Read(self + 0x40, q);
    const auto yaw = read ? gc::CameraYawRadians({q[0], q[1], q[2], q[3]}) : std::nullopt;
    std::lock_guard lock(poseMutex);
    tpsPose = {yaw ? std::optional<double>(std::fmod(*yaw * 180.0 / 3.141592653589793 + 360.0, 360.0)) : std::nullopt,
               GetTickCount64(), true};
}
extern "C" void gc_HeadingAim(uintptr_t self) noexcept {
    if (faulted.load()) return;
    // Aim's slot 4 shares the native output builder with TPS. Offline code at
    // 0x1399d56..0x1399d88 writes its rotation quaternion at +0x40. Observe
    // the previous completed output, just as the existing TPS pre-thunk does.
    // Native Aim update/output gates aiming on owner +0xd1; don't let an
    // inactive mode's quaternion override normal exploration.
    unsigned char aiming = 0;
    const auto owner = Pointer(self + 8);
    const bool active = owner && Read(owner + 0xd1, aiming) && aiming != 0;
    float q[4]{};
    const auto yaw = Read(self + 0x40, q)
        ? gc::CameraYawRadians({q[0],q[1],q[2],q[3]}) : std::nullopt;
    static std::atomic<bool> seen{false};
    if (!seen.exchange(true)) Log("Aim output callback observed; native rotation candidate at +0x40.\r\n");
    std::lock_guard lock(poseMutex);
    aimPose = {yaw ? std::optional<double>(std::fmod(*yaw * 180.0 / 3.141592653589793 + 360.0, 360.0)) : std::nullopt,
               GetTickCount64(), active};
    actionPose = aimPose;
    actionPose.active = true; // A fresh native output also serves non-bow actions.
    actionSource = 5;
}
extern "C" void gc_HeadingAction(uintptr_t self) noexcept {
    if (faulted.load()) return;
    const auto vt = Pointer(self);
    const int source = vt == base + gc::native_build::blackHoleSlot - 4*8 ? 3
        : vt == base + gc::native_build::cinematicSlot - 4*8 ? 4
        : vt == base + gc::native_build::twoTargetSlot - 4*8 ? 6 : 0;
    if (!source) return;
    float q[4]{};
    const auto yaw = Read(self + 0x40, q)
        ? gc::CameraYawRadians({q[0],q[1],q[2],q[3]}) : std::nullopt;
    std::lock_guard lock(poseMutex);
    actionPose = {yaw ? std::optional<double>(std::fmod(*yaw * 180.0 / 3.141592653589793 + 360.0, 360.0)) : std::nullopt,
                  GetTickCount64(), true};
    actionSource = source;
    static bool seenTwoTarget = false; // Accessed only while holding poseMutex.
    if (source == 6 && !seenTwoTarget) {
        seenTwoTarget = true;
        Log("TwoTargetLockOn output observed; reading native rotation at +0x40.\r\n");
    }
}
extern "C" void gc_HeadingTick(uintptr_t self) noexcept {
    if (faulted.load()) return;
    static std::atomic<DWORD> owner{0};
    DWORD expected = 0;
    const auto thread = GetCurrentThreadId();
    owner.compare_exchange_strong(expected, thread);
    if (owner.load() != thread) {
        faulted = true; Log("STOPPED: minimap updates arrived on multiple threads.\r\n"); return;
    }
    static ULONGLONG last = 0;
    const auto now = GetTickCount64();
    if (now - last < 33) return;
    last = now;
    uint32_t labelClass = 0;
    if (!LabelClass(labelClass)) { faulted = true; Log("STOPPED: class lookup failed.\r\n"); return; }
    unsigned budget = 2048;
    const auto label = FindLabel(Pointer(self + 8), labelClass, 0, budget);
    if (!label) return;
    ObserveQuestData(self,now);
    char text[80];
    std::optional<unsigned> step;
    std::optional<double> currentHeading;
    {
        std::lock_guard lock(poseMutex);
        const auto selected = gc::SelectHeading(tpsPose, aimPose, now, actionPose);
        static int lastSource = -1;
        const int source = selected.degrees ? (selected.aiming ? 2 : selected.action ? actionSource : 1) : 0;
        if (source != lastSource) {
            lastSource = source;
            char evidence[120];
            sprintf_s(evidence, "HEADING_SOURCE: %s yaw=%.2f\r\n",
                source == 2 ? "Aim" : source == 1 ? "TPS" : source == 3 ? "BlackHole"
                    : source == 4 ? "CinematicLockOn" : source == 5 ? "Aim action output"
                    : source == 6 ? "TwoTargetLockOn" : "unavailable",
                selected.degrees.value_or(0));
            Log(evidence);
        }
        if (selected.degrees) {
            const auto yawDegrees = *selected.degrees;
            step = gc::DirectionStripStep(yawDegrees);
            currentHeading=yawDegrees;
            sprintf_s(text, "HEADING TEST  %03d deg", static_cast<int>(std::lround(yawDegrees)) % 360);
        } else strcpy_s(text, "Awaiting heading");
    }
    uint32_t stripClass = 0;
    if (!ClassId("gc-direction-strip", stripClass)) { faulted = true; return; }
    budget = 2048;
    const auto strip = FindLabel(Pointer(self + 8), stripClass, 0, budget);
    if (strip) {
        uint32_t desired = 0;
        if (step) {
            if (!yawClasses[*step]) {
                char name[32]; sprintf_s(name, "gc-yaw-%03u", *step);
                if (!ClassId(name, yawClasses[*step])) { faulted = true; return; }
            }
            desired = yawClasses[*step];
            text[0] = '\0';
        } else {
            if (!unavailableClass && !ClassId("gc-strip-unavailable", unavailableClass)) { faulted = true; return; }
            desired = unavailableClass;
        }
        if (!StripState(strip, desired)) {
            faulted = true; Log("STOPPED: compass strip class/restyle failed.\r\n"); return;
        }
    }
    UpdateObjectiveTest(Pointer(self+8),currentHeading);
    if (!Assign(label, text)) { faulted = true; Log("STOPPED: text update faulted; observers only forward original calls.\r\n"); return; }
    static bool logged = false;
    if (!logged) { logged = true; Log("Custom compass elements updated on minimap tick.\r\n"); }
}
extern "C" __declspec(dllexport) void InitializeASI() {
    try { std::call_once(once, Initialize); }
    catch (...) { faulted = true; Log("STOPPED: initialization exception.\r\n"); }
}
BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) { module = instance; DisableThreadLibraryCalls(instance); }
    return TRUE;
}
