#pragma once
#include <cstdint>
// Engine addresses for SPEED2.EXE SHA-256 f9dd86c0...e24168a7 ONLY (image base
// 0x00400000). Never use them before identity.cpp has recognized the executable.
// SVP = research/findings/static-vehicle-path.md, SP2 = research/findings/static-pass-2.md.
// "validated" = confirmed in game by docs/telemetry.md (sessions 1-2, modded profile);
// everything else is statically supported only. World units: metres, Z up.
namespace ugworld::nfsu2 {
constexpr std::uintptr_t image_base = 0x00400000;

// SP2 3.1: CALL World::Tick in the main loop, thiscall (ECX = world), one 32-bit
// float dt on the stack, RET 4. Only reached while GameFlow == 6 and not paused.
constexpr std::uintptr_t tick_call_site = 0x00581407;
constexpr std::uint8_t tick_call_bytes[5] = {0xE8, 0xD4, 0x88, 0x08, 0x00};
constexpr std::uintptr_t world_tick = 0x00609CE0;

// .data globals: always mapped, aligned 32-bit values.
constexpr std::uintptr_t game_flow = 0x008654A4;         // SP2 2.1: 1..8, 6 = driving
constexpr std::uintptr_t players_by_number = 0x008900AC; // SVP 1: P[index]
constexpr std::uintptr_t player_count = 0x008900BC;      // SVP 5
constexpr std::uintptr_t race_object = 0x008900D8;       // SP2 2.3: race status = [[this]]
constexpr std::uintptr_t world_flag = 0x00890084;        // SP2 2.3: tick skip candidate
constexpr std::uintptr_t mode_flags = 0x00850078;        // SP2 2.3
constexpr std::uintptr_t event_id = 0x0089E7A0;          // SP2 2.3

// Player object P (0x9C0 bytes). P and V are reallocated on every track load:
// re-read them each tick, never cache them (validated).
constexpr std::uintptr_t player_vehicle = 0x04;          // SVP 1: V = [P+4] (validated)

// Vehicle object V (0x9E0 bytes), SP2 1.3 (model 1 writer 0x005A5540).
constexpr std::uintptr_t vehicle_index = 0x08;
constexpr std::uintptr_t vehicle_controller_type = 0x24; // SP2 1.5: 1 human, 3 AI, 0 none (validated)
constexpr std::uintptr_t vehicle_model_type = 0x28;      // SP2 1.2: 1 while driving (validated)
constexpr std::uintptr_t vehicle_position = 0x60;        // 3 floats, world position, m (validated)
constexpr std::uintptr_t vehicle_velocity = 0x70;        // 3 floats, world velocity, m/s (validated)
constexpr std::uintptr_t vehicle_basis = 0x90;           // rows forward/left/up, orthonormal (validated)
constexpr std::uintptr_t vehicle_basis_stride = 0x10;
constexpr std::uintptr_t vehicle_angular = 0x110;        // 3 floats, angular velocity? (rad/s plausible)
constexpr std::uintptr_t vehicle_heading = 0x40C;        // uint16 = atan2(fwd.y, fwd.x) * 65536/2pi (validated)
constexpr std::uintptr_t vehicle_forward_speed = 0x42C;  // float = velocity . forward, m/s (validated)
constexpr std::uintptr_t vehicle_speed = 0x430;          // float = |velocity|, m/s (validated)
constexpr std::uintptr_t vehicle_full_model = 0x5C0;     // byte, full dynamics armed
}
