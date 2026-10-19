#pragma once
namespace ugworld {
// Appends to %TEMP%\UGWorld-diagnostic.log and OutputDebugString. Never throws.
// Opens the file per call: not for the engine thread's hot path.
void log(const char* text) noexcept;
void logf(const char* format, ...) noexcept;
// Logs name, base and size of every module loaded in the process.
void log_loaded_modules() noexcept;
}
