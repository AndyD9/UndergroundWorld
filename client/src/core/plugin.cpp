#include "identity.hpp"
#include <windows.h>
#include <string>

static_assert(sizeof(void*) == 4, "ASI must be x86");
namespace {
volatile LONG state = 0; // 0 idle, 1 checking, 2 recognized, -1 disabled
void log(const char* text) noexcept {
    OutputDebugStringA(text);
    // Absolute per-user temporary path, never the game's working directory.
    wchar_t directory[MAX_PATH + 1]{};
    const DWORD length = GetTempPathW(MAX_PATH, directory);
    if (!length || length >= MAX_PATH) return;
    try {
        const auto path = std::wstring(directory) + L"UGWorld-diagnostic.log";
        HANDLE file = CreateFileW(path.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                  nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file == INVALID_HANDLE_VALUE) return;
        DWORD written = 0;
        WriteFile(file, text, static_cast<DWORD>(lstrlenA(text)), &written, nullptr);
        CloseHandle(file);
    } catch (...) { /* logging failure must not affect the host */ }
}
}

// Diagnostic entry point, invoked once OUTSIDE loader lock by the DllMain bootstrap
// thread (or explicitly by a test host). It hashes the executable and logs; it never
// touches engine memory, installs hooks, or starts further threads.
extern "C" __declspec(dllexport) DWORD WINAPI UGWorldInitialize() noexcept {
    const LONG previous = InterlockedCompareExchange(&state, 1, 0);
    if (previous != 0) return previous == 2 ? ERROR_SUCCESS : ERROR_NOT_READY;
    try {
        wchar_t path[32768]{};
        DWORD length = GetModuleFileNameW(nullptr, path, 32768);
        if (!length || length >= 32768 || !ugworld::matches_target(std::wstring(path, length))) {
            InterlockedExchange(&state, -1);
            log("UGWorld: unknown/unreadable executable; diagnostic disabled.\r\n");
            return ERROR_BAD_EXE_FORMAT;
        }
        InterlockedExchange(&state, 2);
        log("UGWorld: disk fingerprint recognized; engine access remains disabled.\r\n");
        return ERROR_SUCCESS;
    } catch (...) {
        InterlockedExchange(&state, -1);
        log("UGWorld: initialization failed; disabled.\r\n");
        return ERROR_GEN_FAILURE;
    }
}

namespace {
// Runs outside loader lock: the thread only starts once DllMain has returned.
// It holds its own module reference so an unload cannot pull the code from under it.
DWORD WINAPI bootstrap(LPVOID module) noexcept {
    UGWorldInitialize();
    FreeLibraryAndExitThread(static_cast<HMODULE>(module), 0);
}
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID) {
    if (reason != DLL_PROCESS_ATTACH) return TRUE;
    DisableThreadLibraryCalls(instance);
    // Under loader lock: only take a reference and start a thread, never wait on it.
    HMODULE self = nullptr;
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
                            reinterpret_cast<LPCWSTR>(&bootstrap), &self))
        return TRUE;
    HANDLE thread = CreateThread(nullptr, 0, bootstrap, self, 0, nullptr);
    // On failure the reference is deliberately leaked: FreeLibrary is unsafe in DllMain.
    if (thread) CloseHandle(thread);
    return TRUE;
}
