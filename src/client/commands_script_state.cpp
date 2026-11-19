#include "skeet/client/commands.hpp"
#include <cmath>

namespace skeet::client {
namespace {
float wrapped(float angle) noexcept {
    if (std::abs(angle) > 180.0f) {
        const float turns = std::round(std::abs(angle / 360.0f));
        const float rotation = turns * 360.0f;
        angle = angle < 0.0f ? angle + rotation : angle - rotation;
    }
    return angle;
}
float clamped(float value, float lower, float upper) noexcept {
    // MAXSS/MINSS choose their second input on unordered comparisons.
    value = value > lower ? value : lower;
    return value < upper ? value : upper;
}
bool changed(float proposed, float previous) noexcept {
    return proposed != previous && !std::isnan(proposed);
}
}

ScriptCommandBaseline capture_script_command(ScriptCommand& state, const Command& command,
                                             float move_yaw, std::uint8_t send_packet,
                                             std::int32_t choked_commands,
                                             std::uint32_t flags) noexcept {
    state.choked_commands = choked_commands;
    state.command_number = command.number;
    state.angles = command.angles;
    state.forward_move = command.movement.forward_move;
    state.side_move = command.movement.side_move;
    state.send_packet = send_packet != 0;
    state.force_defensive = 0;
    state.buttons = command.movement.buttons;
    state.weapon_select = command.weapon_select;
    state.weapon_subtype = command.weapon_subtype;
    state.move_yaw = move_yaw;
    state.tick_shift_state = (flags & 0x600) != 0;
    state.state_bit_3 = (flags >> 3) & 1;
    state.state_bit_18 = (flags >> 18) & 1;
    state.state_bit_20 = 0;
    return {state.send_packet, state.tick_shift_state, state.state_bit_18};
}

void apply_script_command(ScriptCommand& state, Command& command, float& move_yaw,
                          std::uint8_t& send_packet, std::uint32_t& flags,
                          ScriptCommandBaseline baseline, bool unrestricted_angles,
                          bool block_roll) noexcept {
    if (changed(state.angles[0], command.angles[0])) {
        if (!unrestricted_angles)
            state.angles[0] = clamped(wrapped(state.angles[0]), -89.0f, 89.0f);
        command.angles[0] = state.angles[0];
    }
    if (changed(state.angles[1], command.angles[1])) {
        if (!unrestricted_angles)
            state.angles[1] = wrapped(state.angles[1]);
        command.angles[1] = state.angles[1];
    }
    if (!block_roll && changed(state.angles[2], command.angles[2])) {
        if (!unrestricted_angles)
            state.angles[2] = clamped(wrapped(state.angles[2]), -50.0f, 50.0f);
        command.angles[2] = state.angles[2];
    }
    if (changed(state.forward_move, command.movement.forward_move)) {
        if (!unrestricted_angles)
            state.forward_move = clamped(state.forward_move, -450.0f, 450.0f);
        command.movement.forward_move = state.forward_move;
    }
    if (changed(state.side_move, command.movement.side_move)) {
        if (!unrestricted_angles)
            state.side_move = clamped(state.side_move, -450.0f, 450.0f);
        command.movement.side_move = state.side_move;
    }
    if (state.send_packet != baseline.send_packet)
        send_packet = state.send_packet != 0;
    command.movement.buttons = state.buttons;
    command.weapon_select = state.weapon_select;
    command.weapon_subtype = state.weapon_subtype;
    if (changed(state.move_yaw, move_yaw))
        move_yaw = wrapped(state.move_yaw);
    if (state.force_defensive != 0)
        flags |= 0x80000;
    if (state.tick_shift_state != baseline.tick_shift_state)
        flags ^= ((std::uint32_t(state.tick_shift_state) << 9) ^ flags) & 0x600;
    if (state.state_bit_18 != baseline.state_bit_18)
        flags ^= ((std::uint32_t(state.state_bit_18) << 18) ^ flags) & 0x40000;
    if (state.state_bit_20 != 0)
        flags |= 0x100000;
}

} // namespace skeet::client
