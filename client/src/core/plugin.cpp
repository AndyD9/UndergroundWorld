#include "identity.hpp"
#include "log.hpp"
#include "../game/frame_hook.hpp"
#include "../game/telemetry.hpp"
#include <windows.h>
#include <string>

static_assert(sizeof(void*) == 4, "ASI must be x86");
using ugworld::log;
namespace {
volatile LONG state = 0; // 0 idle, 1 checking, 2 recognized, -1 disabled

// Read-only telemetry (phase 3b): only after the fingerprint matched. The module is
// pinned first because the hook leaves engine code pointing into it for good.
void start_telemetry() noexcept {
    HMODULE pinned = nullptr;
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
                            reinterpret_cast<LPCWSTR>(&start_telemetry), &pinned)) {
        log("UGWorld: cannot pin module; telemetry disabled.\r\n");
        return;
    }
    ugworld::log_loaded_modules();
    if (!ugworld::telemetry_start()) {
        log("UGWorld: writer thread failed; telemetry disabled.\r\n");
        return;
    }
    ugworld::install_frame_hook();
}
}

// Entry point, invoked once OUTSIDE loader lock by the DllMain bootstrap thread (or
// explicitly by a test host). It hashes the executable; only on a match does it start
// the read-only telemetry, which hooks the frame tick and reads engine memory.
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
        log("UGWorld: disk fingerprint recognized; starting read-only telemetry.\r\n");
        start_telemetry();
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
