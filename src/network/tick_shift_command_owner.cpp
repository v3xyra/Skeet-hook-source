#include "skeet/network/tick_shift_command_owner.hpp"
#include "skeet/network/tick_shift_phase1.hpp"
#include "skeet/network/tick_shift_phase7.hpp"
#include "skeet/engine/interface_call.hpp"
#include <bit>
#include <cstring>

namespace skeet::network {
namespace {
template <class T> T field(const void* object, std::ptrdiff_t offset) noexcept {
    T value;
    std::memcpy(&value, static_cast<const std::byte*>(object) + offset, sizeof value);
    return value;
}
std::int32_t add(std::int32_t first, std::int32_t second) noexcept {
    return std::bit_cast<std::int32_t>(std::uint32_t(first) + std::uint32_t(second));
}
void* frame_weapon(const client::CommandFrame& frame) noexcept {
    return reinterpret_cast<void*>(std::uintptr_t(frame.weapon_address));
}
TickShiftRuntimeSettings current_settings(const TickShiftCommandOwner& owner) {
    const auto& base = owner.settings;
    return {owner.controls,
            base.command_block,
            base.special_weapon_43467560,
            base.configured_budget_43469274,
            base.process_milliseconds_43468290,
            base.health_divisor_4346db30};
}
void refresh_timing(TickShiftCommandOwner& owner) {
    auto& bindings = owner.bindings;
    auto& timing = bindings.timing;
    timing.choked = *bindings.choked_commands;
    timing.last_outgoing = *bindings.last_outgoing;
    timing.process_milliseconds = owner.settings.process_milliseconds_43468290;
    timing.interval = *bindings.tick_interval;
    timing.current_time = *bindings.current_time;
    timing.latency = owner.timing.latency;
    timing.command_tick = owner.timing.command_tick;
    const auto acknowledged = owner.timing.acknowledged;
    const auto sent = owner.timing.sent;
    timing.have_ack = acknowledged != nullptr;
    timing.acknowledged = acknowledged ? *acknowledged : 0;
    timing.have_sent = sent != nullptr;
    timing.sent = sent ? *sent : 0;
}
}

void reset_tick_shift_command_owner(TickShiftCommandOwner& owner) noexcept {
    auto& runtime = owner.runtime;
    auto& state = runtime.network;
    state.available = 0;
    runtime.low_health_120 = 0;
    runtime.generated_commands.fill(-1);
    state.shot_tick = -1;
    runtime.transition_command_110 = 0x7FFFFFFF;
    owner.phases.recharge_ready_command_11c = 0x7FFFFFFF;
    state.shot_command = -1;
    owner.weapon_command_12c = -1;
    for (auto& entry : runtime.command_ticks)
        entry.command = -1;
    runtime.field_130 = 0;
    runtime.last_flush_command_134 = -1;
}

void tick_shift_owner_phase1(TickShiftCommandOwner& owner, void* player, void*, client::Command& command,
                          std::uint8_t& send_packet) {
    const auto settings = current_settings(owner);
    process_tick_shift_phase1(owner.runtime, owner.phases, player, command, send_packet, settings,
                           owner.bindings, owner.phase1, owner.command_flags);
}

void tick_shift_owner_phase2(TickShiftCommandOwner& owner, void*, void* weapon, client::Command& command,
                          std::uint8_t&, std::array<float, 3>&) {
    auto& state = owner.runtime.network;
    auto& bindings = owner.bindings;
    state.flags &= ~0x80U;
    if (state.mode == 0)
        return;
    if (!owner.settings.special_weapon_43467560) {
        if (weapon && engine::invoke<std::int32_t>(weapon, 455) == 9) {
            if ((command.movement.buttons & 0x801U) ||
                field<std::uint8_t>(weapon, bindings.offsets.weapon_byte_e1df7e9b) ||
                field<float>(weapon, bindings.offsets.weapon_time_3b93667b) > 0.0f ||
                owner.weapon_command_12c == *bindings.last_outgoing) {
                state.flags = (state.flags & ~2U) | 0x80U;
                owner.weapon_command_12c = *bindings.last_outgoing;
                return;
            }
        } else if (owner.weapon_command_12c == *bindings.last_outgoing) {
            state.flags = (state.flags & ~2U) | 0x80U;
            return;
        }
    }
    if ((state.flags & 2U) || !weapon)
        return;
    const auto identity = engine::invoke<std::int32_t>(static_cast<std::byte*>(weapon) + 8, 10);
    if (command.weapon_select && command.weapon_select != identity) {
        state.flags |= 2U;
        return;
    }
    auto number = *bindings.last_outgoing;
    for (std::uint32_t count = 0; count < 150; ++count) {
        number = add(number, 1);
        if (command.number <= number)
            return;
        const auto& historical = bindings.commands[std::uint32_t(number) % 150];
        if (historical.number == number && historical.weapon_select &&
            historical.weapon_select != identity) {
            state.flags |= 2U;
            return;
        }
    }
}

void tick_shift_owner_phase3(TickShiftCommandOwner& owner, client::CommandFrame& frame, void*,
                          client::Command&, std::uint8_t& send_packet) {
    apply_tick_shift_weapon_gate(owner.runtime.network, *owner.bindings.last_outgoing,
                              owner.settings.special_weapon_43467560, frame.weapon_kind_124,
                              frame.choked_15c, frame.secondary_flags, owner.command_flags,
                              send_packet);
}

void tick_shift_owner_phase4(TickShiftCommandOwner& owner, client::CommandFrame& frame, void* player,
                          client::Command& command, std::uint8_t&) {
    refresh_timing(owner);
    auto& timing = owner.bindings.timing;
    timing.player_next_attack = timing.weapon_next_attack = 0.0f;
    const auto& state = owner.runtime.network;
    if (state.mode != 0 && state.available >= state.limit && frame.weapon_address) {
        timing.player_next_attack =
            field<float>(player, owner.bindings.firing.offsets.player_next_attack);
        timing.weapon_next_attack =
            field<float>(frame_weapon(frame), owner.bindings.firing.offsets.next_primary);
    }
    timing.freeze_period = false;
    if (state.mode == 3 && state.available >= state.limit) {
        const auto rules = owner.bindings.game_rules ? *owner.bindings.game_rules : nullptr;
        timing.freeze_period =
            rules && field<std::uint8_t>(rules, owner.bindings.firing.offsets.rule_d720aea6);
    }
    TickShiftFrame projected{frame.flags,
                           frame.previous_command.movement.buttons,
                           frame.weapon_address != 0,
                           frame.command_number_ac,
                           frame.tick_base_c8,
                           std::bit_cast<std::int32_t>(frame.field_cc),
                           frame.weapon_id_d8,
                           frame.weapon_kind_124,
                           std::bit_cast<std::int32_t>(frame.field_1d4),
                           std::bit_cast<std::int32_t>(frame.field_1d8),
                           std::bit_cast<std::int32_t>(frame.field_1e0)};
    TickShiftCommand input{command.number, command.angles[0], command.angles[1],
                         command.movement.buttons};
    update_tick_shift_command(owner.runtime.network, projected, input, owner.bindings.timing,
                           owner.command_flags);
    frame.flags = projected.flags;
    frame.field_cc = std::uint32_t(projected.saved_tick_base);
    frame.field_1d4 = std::uint32_t(projected.tick_shift_mode);
    frame.field_1d8 = std::uint32_t(projected.next_tick);
    frame.field_1e0 = std::uint32_t(projected.choke_end);
    command.angles[0] = input.pitch;
    command.angles[1] = input.yaw;
    command.movement.buttons = input.buttons;
}

void tick_shift_owner_phase5(TickShiftCommandOwner& owner, client::CommandFrame& frame, void* player,
                          client::Command& command, std::uint8_t& send_packet) {
    refresh_timing(owner);
    const auto settings = current_settings(owner);
    process_tick_shift_runtime(owner.runtime, frame, player, frame_weapon(frame), command, send_packet,
                            settings, owner.bindings, owner.command_flags);
}

void tick_shift_owner_phase7(TickShiftCommandOwner& owner, client::CommandFrame& frame, void* player,
                          client::Command& command, std::uint8_t& send_packet) {
    refresh_timing(owner);
    const auto settings = current_settings(owner);
    process_tick_shift_phase7(owner.runtime, owner.phases, frame, player, command, send_packet,
                           settings, owner.bindings, owner.phase7, owner.command_flags);
}
} // namespace skeet::network
