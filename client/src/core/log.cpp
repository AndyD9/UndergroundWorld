#include "log.hpp"
#include <windows.h>
#include <tlhelp32.h>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <string>

namespace ugworld {
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

void logf(const char* format, ...) noexcept {
    char buffer[1024];
    va_list args;
    va_start(args, format);
    const int written = std::vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    if (written > 0) log(buffer);
}

void log_loaded_modules() noexcept {
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, 0);
    if (snapshot == INVALID_HANDLE_VALUE) return;
    MODULEENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    for (BOOL more = Module32FirstW(snapshot, &entry); more; more = Module32NextW(snapshot, &entry))
        logf("UGWorld: module %ls base 0x%08X size 0x%X\r\n", entry.szModule,
             static_cast<unsigned>(reinterpret_cast<std::uintptr_t>(entry.modBaseAddr)),
             static_cast<unsigned>(entry.modBaseSize));
    CloseHandle(snapshot);
}
}
