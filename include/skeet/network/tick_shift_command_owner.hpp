#pragma once
#include "tick_shift_runtime.hpp"

namespace skeet::network {

struct TickShiftPhaseState;
struct TickShiftPhase1Bindings;
struct TickShiftPhase7Bindings;

// Live source words used by43385D57/4340C3FA. Optional ack/sent pointers are
// the actual pointer cells, so a later phase observes engine replacement.
struct TickShiftOwnerTiming {
    const LatencySamples& latency;
    const std::int32_t& command_tick;
    const std::int32_t* const& acknowledged;
    const std::int32_t* const& sent;
};
struct TickShiftCommandOwner {
    TickShiftRuntimeState& runtime;
    const TickShiftRuntimeSettings& settings;
    TickShiftRuntimeBindings& bindings;
    std::uint32_t& command_flags; // shared434759C0
    TickShiftOwnerTiming timing;
    TickShiftPhaseState& phases;
    TickShiftPhase1Bindings& phase1;
    TickShiftPhase7Bindings& phase7;
    std::int32_t weapon_command_12c = 0;
    const TickShiftControlView& controls;
};

// Complete primary434707D0 slot5. Ring ticks and history are preserved;
// only their native command tags and the explicit sparse scalar fields reset.
void reset_tick_shift_command_owner(TickShiftCommandOwner&) noexcept; //4337D660

void tick_shift_owner_phase1(TickShiftCommandOwner&, void* player, void* weapon, client::Command&,
                          std::uint8_t& send_packet); //433ABC86
// Concrete433D6A11. The shared phase2 contract carries player/send/angles;
// this native owner only consumes weapon and command on that phase.
void tick_shift_owner_phase2(TickShiftCommandOwner&, void* player, void* weapon, client::Command&,
                          std::uint8_t& send_packet, std::array<float, 3>& incoming_angles);
void tick_shift_owner_phase3(TickShiftCommandOwner&, client::CommandFrame&, void* player,
                          client::Command&, std::uint8_t& send_packet); //433B56ED
void tick_shift_owner_phase4(TickShiftCommandOwner&, client::CommandFrame&, void* player,
                          client::Command&, std::uint8_t& send_packet); //433CE4BC
void tick_shift_owner_phase5(TickShiftCommandOwner&, client::CommandFrame&, void* player,
                          client::Command&, std::uint8_t& send_packet); //4336663F
void tick_shift_owner_phase7(TickShiftCommandOwner&, client::CommandFrame&, void* player,
                          client::Command&, std::uint8_t& send_packet); //43359E37

} // namespace skeet::network
