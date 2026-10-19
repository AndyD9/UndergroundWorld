#pragma once
#include <cstdint>
namespace ugworld {
// Raw per-tick snapshot captured on the engine thread, copied by value. Floats are
// kept as their 32-bit patterns (no FPU use while capturing); player and vehicle
// are opaque identities for change detection, never dereferenced outside `game`.
struct TelemetrySample {
    std::uint32_t frame;
    std::uint32_t dt_bits;
    std::int32_t game_flow, player_count, race_status, world_flag, mode_flags, event_id;
    std::uint32_t status;
    std::uint32_t player, vehicle, vehicle_index;
    std::int32_t controller_type, model_type;
    std::uint32_t full_model, heading;
    std::uint32_t position[3], velocity[3];
    std::uint32_t speed, forward_speed;
    std::uint32_t basis[3][3];
    std::uint32_t angular[3];
};
enum : std::uint32_t { sample_ok = 0, sample_no_player = 1, sample_no_vehicle = 2, sample_fault = 3 };

// Engine thread only, from the post-tick hook: never blocks, allocates or throws.
void telemetry_on_tick(std::uint32_t dt_bits) noexcept;
// Starts the writer thread: CSV in %TEMP%, GameFlow transitions in the diagnostic log.
bool telemetry_start() noexcept;
}
