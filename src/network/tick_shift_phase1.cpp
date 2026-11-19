#include "skeet/network/tick_shift_phase1.hpp"
#include "skeet/network/tick_shift_controls.hpp"
#include "skeet/engine/interface_call.hpp"
#include "skeet/misc/grenade_owner.hpp"
#include <algorithm>
#include <bit>
#include <cstring>
#include <immintrin.h>
#include <limits>
#include <stdexcept>

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
std::int32_t sub(std::int32_t first, std::int32_t second) noexcept {
    return std::bit_cast<std::int32_t>(std::uint32_t(first) - std::uint32_t(second));
}
void force_send(std::uint32_t& flags) noexcept {
    flags = (flags & ~0x400U) | 0x200U;
}
std::int32_t time_ticks(float current, float interval) noexcept {
    return _mm_cvttss_si32(
        _mm_add_ss(_mm_div_ss(_mm_set_ss(current), _mm_set_ss(interval)), _mm_set_ss(0.5f)));
}
void select_mode(TickShiftState& state, void* player, const TickShiftRuntimeSettings& settings,
                 const TickShiftRuntimeBindings& bindings, const std::uint32_t& command_flags) {
    const auto rules = bindings.game_rules ? *bindings.game_rules : nullptr;
    const auto& offsets = bindings.offsets;
    const bool rule0 = rules && field<std::uint8_t>(rules, offsets.rule_bf567619);
    const bool rule1 = rule0 && field<std::uint8_t>(rules, offsets.rule_dda99ab5);
    const auto rule_mode = rule1 ? field<std::int32_t>(rules, offsets.rule_b7ea8fda) : 0;
    state.budget = network_command_budget(rules != nullptr, rule0, rule1, rule_mode,
                                          settings.configured_budget_43469274);
    state.mode = 0;
    state.minimum = sub(state.budget, 1);
    if (!engine::invoke<bool>(bindings.engine_client, 26) || !bindings.checksum || state.budget < 7)
        return;
    const auto freeze_rules = bindings.game_rules ? *bindings.game_rules : nullptr;
    if (freeze_rules && field<std::uint8_t>(freeze_rules, bindings.firing.offsets.rule_d720aea6) &&
        bindings.freeze_entity_43475b74 &&
        field<std::int32_t>(bindings.freeze_entity_43475b74, 0x20) > 1)
        return;
    if (field<std::uint8_t>(player, offsets.player_block_9d920e35))
        return;
    if ((command_flags & 0x40U) ||
        hotkey_active(settings.command_block, tick_shift_control_keys(settings)) ||
        (command_flags & 0x100U))
        return;
    if (tick_shift_control_active(settings, TickShiftControl::fake_peek))
        state.mode = 3;
    else if (tick_shift_control_active(settings, TickShiftControl::double_tap))
        state.mode = 1;
    else if (tick_shift_control_active(settings, TickShiftControl::on_shot))
        state.mode = 2;
    else
        return;
    state.minimum = (std::max)(1, tick_shift_fake_lag_setting(settings));
    state.limit = std::clamp(sub(sub(state.budget, tick_shift_fake_lag_setting(settings)), 1), 1, 15);
    if (state.mode == 2)
        state.flags &= ~1U;
}
void* active_weapon(void* player, const TickShiftPhase1Bindings& bindings) {
    const auto handle = field<std::uint32_t>(player, bindings.active_weapon_cd812654);
    return handle == 0xFFFFFFFFU ? nullptr : engine::invoke<void*>(bindings.entity_list, 4, handle);
}
}

bool tick_shift_phase1_record_pending(const TickShiftPhase1Bindings& bindings, float interval) {
    const auto records = misc::grenade_owner_warning_records(bindings.record_owner_43475aac);
    for (const auto& record : records) {
        if (record.type != 0 || !(record.damage > 0.0f) || record.handle == UINT32_MAX)
            continue;
        const auto tick_time = _mm_mul_ss(
            _mm_cvtepi32_ps(_mm_cvtsi32_si128(bindings.tick_43475a60)), _mm_set_ss(interval));
        const auto remaining = _mm_sub_ss(_mm_set_ss(record.end_time), tick_time);
        if (0.5f >= _mm_cvtss_f32(remaining))
            return true;
    }
    return false;
}

void notify_tick_shift_recharge_observers(const TickShiftPhase1Bindings& bindings, void* player,
                                       std::int32_t command_number) {
    notify_tick_shift_recharge_observers(bindings.recharge_observers, player, command_number);
}

void process_tick_shift_phase1(TickShiftRuntimeState& runtime, TickShiftPhaseState& phases, void* player,
                            client::Command& command, std::uint8_t& send_packet,
                            const TickShiftRuntimeSettings& settings,
                            const TickShiftRuntimeBindings& native,
                            const TickShiftPhase1Bindings& bindings, std::uint32_t& command_flags) {
    auto& state = runtime.network;
    if (*native.choked_commands == 0)
        state.flags = (state.flags & ~0x54U) | ((state.flags & 2U) << 1) | 3U;
    command_flags &= ~0x800U;
    state.flags &= ~0x20U;
    select_mode(state, player, settings, native, command_flags);
    if (state.mode == 0) {
        if (state.available > 0)
            state.minimum = (std::max)(1, tick_shift_fake_lag_setting(settings));
        state.timer_124 = 0.0f;
        if (state.available > 0)
            force_send(command_flags);
        return;
    }
    const auto now = time_ticks(*native.current_time, *native.tick_interval);
    if (now < state.shot_tick && sub(state.shot_tick, now) < 16) {
        state.flags |= 0x20U;
        force_send(command_flags);
    }
    if (state.available >= state.limit) {
        const auto remaining = (std::max)(0, sub(state.budget, *native.choked_commands));
        if (state.available > remaining)
            state.available = remaining;
    }
    if (*native.choked_commands != 0)
        return;
    if (command_flags & 0x200000U) {
        command_flags &= ~0x200000U;
        state.timer_124 = 0.0f;
    }
    if (command.movement.buttons & 0x801U) {
        auto* weapon = active_weapon(player, bindings);
        if (!weapon || engine::invoke<std::int32_t>(weapon, 455) != 9)
            return;
    }
    if (tick_shift_phase1_record_pending(bindings, *native.tick_interval))
        return;
    // The working 433AC04B..433AC067 path retains the hotkey query, then
    // continues at 433AC16D without the command-flag or target veto.
    if (bindings.rage.enabled.value)
        (void)hotkey_active(bindings.rage.activation.value.binding, tick_shift_control_keys(settings));
    if (state.timer_124 > 0.0f)
        return;
    const auto acknowledged = *native.command_ack;
    if (acknowledged != phases.observed_ack_10c) {
        phases.observed_ack_10c = acknowledged;
        if (state.available >= state.limit && acknowledged >= phases.recharge_ready_command_11c &&
            state.history_count == 8 && !(state.flags & 8U)) {
            std::int32_t maximum = 0;
            for (std::int32_t index = 0; index < state.history_count; ++index)
                maximum =
                    (std::max)(maximum,
                               state.history[(state.history_first + std::uint32_t(index)) & 7U]);
            if (state.available > maximum && sub(state.available, maximum) >= 2) {
                state.history_count = 0;
                state.history_first = 0;
                state.available = maximum;
                phases.recharge_ready_command_11c = (std::numeric_limits<std::int32_t>::max)();
            }
        }
    }
    if (state.available >= state.limit)
        return;
    // Working 433AC224 -> 434230A7 preserves weapon changes as real commands.
    if (command.weapon_select != 0)
        return;
    command.tick = 0x7F7FFFFF;
    state.available = add(state.available, 1);
    state.history_count = 0;
    state.history_first = 0;
    if (state.available >= state.limit) {
        phases.recharge_ready_command_11c = add(command.number, 1);
        state.last_command = command.number;
    }
    force_send(command_flags);
    send_packet = 1;
    notify_tick_shift_recharge_observers(bindings, player, command.number);
}
} // namespace skeet::network
