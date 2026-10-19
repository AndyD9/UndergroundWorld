#include "telemetry.hpp"
#include "frame_hook.hpp"
#include "nfsu2.hpp"
#include "../core/log.hpp"
#include <windows.h>
#include <array>
#include <atomic>
#include <climits>
#include <cstdio>
#include <cstring>
#include <string>

namespace ugworld {
namespace {
// Single producer (engine thread), single consumer (writer thread); drops when full.
class SampleRing {
public:
    bool push(const TelemetrySample& sample) noexcept {
        const auto head = head_.load(std::memory_order_relaxed);
        if (head - tail_.load(std::memory_order_acquire) == capacity) return false;
        slots_[head % capacity] = sample;
        head_.store(head + 1, std::memory_order_release);
        return true;
    }
    bool pop(TelemetrySample& sample) noexcept {
        const auto tail = tail_.load(std::memory_order_relaxed);
        if (tail == head_.load(std::memory_order_acquire)) return false;
        sample = slots_[tail % capacity];
        tail_.store(tail + 1, std::memory_order_release);
        return true;
    }
private:
    static constexpr std::uint32_t capacity = 4096; // about a minute at 60 FPS
    std::array<TelemetrySample, capacity> slots_{};
    std::atomic<std::uint32_t> head_{0}, tail_{0};
};

SampleRing ring;
std::uint32_t frame_counter = 0; // engine thread only
std::atomic<std::uint32_t> dropped{0}, faults{0};

template <typename T> T read(std::uintptr_t address) noexcept {
    return *reinterpret_cast<const volatile T*>(address);
}

// SEH turns a stale pointer into one faulted sample instead of a game crash.
// Raw 32-bit copies only: the engine's x87 and MXCSR state stay untouched.
void capture(TelemetrySample& s) noexcept {
    __try {
        s.game_flow = read<std::int32_t>(nfsu2::game_flow);
        s.player_count = read<std::int32_t>(nfsu2::player_count);
        s.world_flag = read<std::int32_t>(nfsu2::world_flag);
        s.mode_flags = read<std::int32_t>(nfsu2::mode_flags);
        s.event_id = read<std::int32_t>(nfsu2::event_id);
        const auto race = read<std::uintptr_t>(nfsu2::race_object);
        s.race_status = race ? read<std::int32_t>(race) : INT_MIN;

        const auto player = read<std::uintptr_t>(nfsu2::players_by_number);
        s.player = player;
        if (!player) { s.status = sample_no_player; return; }
        const auto vehicle = read<std::uintptr_t>(player + nfsu2::player_vehicle);
        s.vehicle = vehicle;
        if (!vehicle) { s.status = sample_no_vehicle; return; }

        s.vehicle_index = read<std::uint32_t>(vehicle + nfsu2::vehicle_index);
        s.controller_type = read<std::int32_t>(vehicle + nfsu2::vehicle_controller_type);
        s.model_type = read<std::int32_t>(vehicle + nfsu2::vehicle_model_type);
        s.full_model = read<std::uint8_t>(vehicle + nfsu2::vehicle_full_model);
        s.heading = read<std::uint16_t>(vehicle + nfsu2::vehicle_heading);
        for (std::uintptr_t i = 0; i < 3; ++i) {
            s.position[i] = read<std::uint32_t>(vehicle + nfsu2::vehicle_position + 4 * i);
            s.velocity[i] = read<std::uint32_t>(vehicle + nfsu2::vehicle_velocity + 4 * i);
            s.angular[i] = read<std::uint32_t>(vehicle + nfsu2::vehicle_angular + 4 * i);
            for (std::uintptr_t j = 0; j < 3; ++j)
                s.basis[i][j] = read<std::uint32_t>(
                    vehicle + nfsu2::vehicle_basis + nfsu2::vehicle_basis_stride * i + 4 * j);
        }
        s.speed = read<std::uint32_t>(vehicle + nfsu2::vehicle_speed);
        s.forward_speed = read<std::uint32_t>(vehicle + nfsu2::vehicle_forward_speed);
        s.status = sample_ok;
    } __except (GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION
                    ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH) {
        s.status = sample_fault;
    }
}

void append_float(std::string& out, std::uint32_t bits) {
    float value;
    std::memcpy(&value, &bits, sizeof(value));
    char text[32];
    std::snprintf(text, sizeof(text), ",%.7g", value);
    out += text;
}

void append_row(std::string& out, const TelemetrySample& s) {
    char text[256];
    std::snprintf(text, sizeof(text), "%u", s.frame);
    out += text;
    append_float(out, s.dt_bits);
    std::snprintf(text, sizeof(text), ",%d,%d,%d,%d,0x%X,0x%X,%u,0x%08X,0x%08X,%u,%d,%d,%u,%u",
                  s.game_flow, s.player_count, s.race_status, s.world_flag, s.mode_flags,
                  s.event_id, s.status, s.player, s.vehicle, s.vehicle_index,
                  s.controller_type, s.model_type, s.full_model, s.heading);
    out += text;
    for (auto v : s.position) append_float(out, v);
    for (auto v : s.velocity) append_float(out, v);
    append_float(out, s.speed);
    append_float(out, s.forward_speed);
    for (auto& row : s.basis) for (auto v : row) append_float(out, v);
    for (auto v : s.angular) append_float(out, v);
    out += "\r\n";
}

constexpr char csv_header[] =
    "frame,dt,game_flow,player_count,race_status,world_flag,mode_flags,event_id,status,"
    "player,vehicle,vehicle_index,controller_type,model_type,full_model,heading,"
    "pos_x,pos_y,pos_z,vel_x,vel_y,vel_z,speed,forward_speed,"
    "b0_x,b0_y,b0_z,b1_x,b1_y,b1_z,b2_x,b2_y,b2_z,ang_x,ang_y,ang_z\r\n";

HANDLE open_csv() noexcept {
    wchar_t directory[MAX_PATH + 1]{};
    const DWORD length = GetTempPathW(MAX_PATH, directory);
    if (!length || length >= MAX_PATH) return INVALID_HANDLE_VALUE;
    SYSTEMTIME now{};
    GetLocalTime(&now);
    wchar_t path[MAX_PATH + 64]{};
    swprintf_s(path, L"%sUGWorld-telemetry-%04u%02u%02u-%02u%02u%02u.csv", directory,
               now.wYear, now.wMonth, now.wDay, now.wHour, now.wMinute, now.wSecond);
    HANDLE file = CreateFileW(path, GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_NEW,
                              FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file != INVALID_HANDLE_VALUE) {
        DWORD written = 0;
        WriteFile(file, csv_header, sizeof(csv_header) - 1, &written, nullptr);
        logf("UGWorld: telemetry file %ls\r\n", path);
    }
    return file;
}

DWORD WINAPI writer(LPVOID) noexcept {
    HANDLE file = open_csv();
    if (file == INVALID_HANDLE_VALUE) log("UGWorld: telemetry file unavailable; samples discarded.\r\n");
    int last_flow = INT_MIN;
    std::uint32_t last_vehicle = 0, reported_dropped = 0, reported_faults = 0;
    bool checked_while_driving = false;
    for (;;) {
        Sleep(100);
        try {
            // Static .data global, aligned: safe to read from this thread. Transitions
            // happen outside the tick, so the engine thread cannot report them.
            const int flow = read<std::int32_t>(nfsu2::game_flow);
            if (flow != last_flow) {
                logf("UGWorld: GameFlow %d -> %d (players %d, mode 0x%X, event 0x%X)\r\n",
                     last_flow, flow, read<std::int32_t>(nfsu2::player_count),
                     read<std::int32_t>(nfsu2::mode_flags), read<std::int32_t>(nfsu2::event_id));
                last_flow = flow;
                if (flow == 6 && !checked_while_driving) {
                    // Other ASIs may load or patch after our bootstrap ran.
                    checked_while_driving = true;
                    log_loaded_modules();
                    log(frame_hook_intact() ? "UGWorld: frame hook intact while driving.\r\n"
                                            : "UGWorld: frame hook NOT intact while driving.\r\n");
                }
            }
            std::string batch;
            TelemetrySample sample;
            while (ring.pop(sample)) {
                if (sample.vehicle != last_vehicle) {
                    logf("UGWorld: frame %u player 0x%08X vehicle 0x%08X -> 0x%08X (status %u)\r\n",
                         sample.frame, sample.player, last_vehicle, sample.vehicle, sample.status);
                    last_vehicle = sample.vehicle;
                }
                append_row(batch, sample);
            }
            if (file != INVALID_HANDLE_VALUE && !batch.empty()) {
                DWORD written = 0;
                WriteFile(file, batch.data(), static_cast<DWORD>(batch.size()), &written, nullptr);
            }
            const auto now_dropped = dropped.load(std::memory_order_relaxed);
            const auto now_faults = faults.load(std::memory_order_relaxed);
            if (now_dropped != reported_dropped || now_faults != reported_faults) {
                logf("UGWorld: telemetry dropped %u, faulted %u samples so far\r\n", now_dropped, now_faults);
                reported_dropped = now_dropped;
                reported_faults = now_faults;
            }
        } catch (...) { /* out of memory while formatting: skip this batch */ }
    }
}
}

void telemetry_on_tick(std::uint32_t dt_bits) noexcept {
    TelemetrySample sample{};
    sample.frame = ++frame_counter;
    sample.dt_bits = dt_bits;
    capture(sample);
    if (sample.status == sample_fault) faults.fetch_add(1, std::memory_order_relaxed);
    if (!ring.push(sample)) dropped.fetch_add(1, std::memory_order_relaxed);
}

bool telemetry_start() noexcept {
    HANDLE thread = CreateThread(nullptr, 0, writer, nullptr, 0, nullptr);
    if (!thread) return false;
    CloseHandle(thread);
    return true;
}
}
