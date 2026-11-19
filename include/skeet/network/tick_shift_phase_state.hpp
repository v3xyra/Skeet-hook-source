#pragma once
#include <cstdint>
#include <limits>

namespace skeet::network {
// Network subobject fields shared by phase1 and phase7. Constructor4347DEEC
// writes both words through the complete owner's +124/+134 offsets.
struct TickShiftPhaseState {
    std::int32_t observed_ack_10c = -1;
    std::int32_t recharge_ready_command_11c = (std::numeric_limits<std::int32_t>::max)();
};
} // namespace skeet::network
