#include <Windows.h>
int wmain(int argc, wchar_t** argv) {
    if (argc != 2) return 1;
    HMODULE dll = LoadLibraryW(argv[1]);
    if (!dll) return 2;
    auto init = reinterpret_cast<void(*)()>(GetProcAddress(dll, "InitializeASI"));
    if (!init) return 3;
    // No opt-in INI beside the build output: initialization must remain disabled.
    init(); init();
    return FreeLibrary(dll) ? 0 : 4;
}
