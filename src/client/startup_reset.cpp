#include "skeet/client/startup_reset.hpp"
#include <algorithm>

namespace skeet::client {
void reset_target_state(TargetResetState state) noexcept { // 4341D854
    state.handle = -1;
    state.value_2c = 0;
    state.value_30 = 0;
    state.value_38 = 0;
}

void reset_command_state(CommandResetState state) noexcept { // 433D6630
    state.flags &= 0xfffef9e7u;
    state.value_c4 = -1;
    std::fill(state.inverted_commands.begin(), state.inverted_commands.end(), 0xffffffffu);
    state.value_cc = -1;
    state.value_c8 = -1;
    state.value_dc = -1;
    state.value_e8 = -1;
    state.value_f8 = -1;
    reset_target_state(state.target);
}

void reset_startup_acknowledgements(std::span<StartupAcknowledgement, 150> records) noexcept {
    for (auto& record : records)
        record[0] = 0xffffffffu;
}

void request_initial_connection_refresh(std::uint8_t& requested, std::uint8_t& pending,
                                        void (*notify)()) {
    // Startup and the first player command can run in either order on an open map.
    // The pending bit can be consumed between them.
    if (requested)
        return;
    requested = 1;
    if (notify)
        notify();
    else
        pending = 1;
}
}
