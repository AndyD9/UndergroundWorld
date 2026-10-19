#pragma once
#include <optional>
#include "../vehicles/vehicle_state.hpp"
namespace ugworld {
// Owned snapshots only. No engine pointers escape this boundary.
class GameAccess {
public:
    virtual ~GameAccess() = default;
    virtual std::optional<VehicleState> read_player() const noexcept = 0;
};
class UnverifiedGameAccess final : public GameAccess {
public:
    std::optional<VehicleState> read_player() const noexcept override { return std::nullopt; }
};
}
