#pragma once
#include <array>
#include <cstdint>
#include <span>

namespace skeet::client {

// Views of shared globals; command, targeting and networking consumers retain
// their original storage. The startup/reset paths do not own separate copies.
struct TargetResetState {
    std::int32_t& handle; // 4346DB1C
    std::uint32_t& value_2c; // 4346DB2C
    std::uint32_t& value_30; // 4346DB30
    std::uint32_t& value_38; // 4346DB38
};
struct CommandResetState {
    std::uint32_t& flags; // 434759C0
    std::int32_t& value_c4; // 434759C4
    std::int32_t& value_c8; // 434759C8
    std::int32_t& value_cc; // 434759CC
    std::int32_t& value_dc; // 434759DC
    std::int32_t& value_e8; // 434759E8
    std::int32_t& value_f8; // 434759F8
    std::span<std::uint32_t, 150> inverted_commands; // 43476090
    TargetResetState target;
};

void reset_target_state(TargetResetState state) noexcept; // 4341D854
void reset_command_state(CommandResetState state) noexcept; // 433D6630

// 4341F780..4341F790 clears only the first dword of each eight-byte record.
using StartupAcknowledgement = std::array<std::uint32_t, 2>;
void reset_startup_acknowledgements(std::span<StartupAcknowledgement, 150> records) noexcept;
void request_initial_connection_refresh(std::uint8_t& requested, std::uint8_t& pending,
                                        void (*notify)());
}
