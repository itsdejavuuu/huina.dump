#include <windows.h>
#include "app/session.hpp"

namespace {

HMODULE g_self = nullptr;

DWORD WINAPI DumpThread(LPVOID) {
    const DWORD code = hd::app::RunGuarded(g_self);
    FreeLibraryAndExitThread(g_self, code);
    __assume(0);
}

}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        g_self = hModule;
        DisableThreadLibraryCalls(hModule);
        HANDLE thread = CreateThread(nullptr, 0, DumpThread, nullptr, 0, nullptr);
        if (thread) CloseHandle(thread);
    }
    return TRUE;
}
