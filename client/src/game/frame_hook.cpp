#include "frame_hook.hpp"
#include "nfsu2.hpp"
#include "telemetry.hpp"
#include "../core/log.hpp"
#include <windows.h>
#include <cstring>

namespace ugworld {
namespace {
constexpr std::uintptr_t rel32_address = nfsu2::tick_call_site + 1;
constexpr std::uintptr_t next_instruction = nfsu2::tick_call_site + 5;
static_assert(rel32_address % 4 == 0, "rel32 must be aligned for an atomic swap");
static_assert(nfsu2::world_tick - next_instruction == 0x000888D4, "rel32 must match the expected bytes");

// __fastcall with an unused EDX is ABI-identical to thiscall with one stack argument
// and callee cleanup (RET 4). dt travels as raw bits so this thread never touches the
// FPU; callee-saved registers and ESP are restored by the compiler-generated epilogue.
using WorldTick = void(__fastcall*)(void* world, void* edx, std::uint32_t dt_bits);

void __fastcall world_tick_hook(void* world, void* edx, std::uint32_t dt_bits) {
    reinterpret_cast<WorldTick>(nfsu2::world_tick)(world, edx, dt_bits);
    telemetry_on_tick(dt_bits);
}

LONG hook_rel32() noexcept {
    return static_cast<LONG>(reinterpret_cast<std::uintptr_t>(&world_tick_hook) - next_instruction);
}
}

bool install_frame_hook() noexcept {
    if (reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr)) != nfsu2::image_base) {
        log("UGWorld: unexpected image base; frame hook refused.\r\n");
        return false;
    }
    const auto* site = reinterpret_cast<const std::uint8_t*>(nfsu2::tick_call_site);
    if (std::memcmp(site, nfsu2::tick_call_bytes, sizeof(nfsu2::tick_call_bytes)) != 0) {
        logf("UGWorld: tick call site holds %02X %02X %02X %02X %02X (another mod?); frame hook refused.\r\n",
             site[0], site[1], site[2], site[3], site[4]);
        return false;
    }
    auto* rel32 = reinterpret_cast<LONG*>(rel32_address);
    DWORD protection = 0;
    if (!VirtualProtect(rel32, sizeof(LONG), PAGE_EXECUTE_READWRITE, &protection)) {
        log("UGWorld: VirtualProtect failed; frame hook refused.\r\n");
        return false;
    }
    // One aligned interlocked swap: the engine thread sees either the old or new target.
    const LONG expected = static_cast<LONG>(nfsu2::world_tick - next_instruction);
    const LONG previous = InterlockedCompareExchange(rel32, hook_rel32(), expected);
    DWORD ignored = 0;
    VirtualProtect(rel32, sizeof(LONG), protection, &ignored);
    FlushInstructionCache(GetCurrentProcess(), rel32, sizeof(LONG));
    if (previous != expected) {
        log("UGWorld: tick call site changed during install; frame hook refused.\r\n");
        return false;
    }
    logf("UGWorld: frame hook installed at 0x%08X.\r\n", static_cast<unsigned>(nfsu2::tick_call_site));
    return true;
}

bool frame_hook_intact() noexcept {
    return *reinterpret_cast<const volatile std::uint8_t*>(nfsu2::tick_call_site) == 0xE8 &&
           *reinterpret_cast<const volatile LONG*>(rel32_address) == hook_rel32();
}
}
