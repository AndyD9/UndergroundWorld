#pragma once
#include <array>
namespace ugworld {
// Application-owned proposal, NOT a reverse-engineered game memory layout.
// Units, axes and quaternion order must be established before populating it.
struct VehicleState {
    std::array<float, 3> position{};
    std::array<float, 4> orientation{};
    std::array<float, 3> velocity{};
};
}
