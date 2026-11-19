#include "skeet/network/tick_shift_phase7.hpp"
#include "skeet/network/tick_shift_phase_state.hpp"
#include "skeet/anti_aim/config.hpp"
#include "skeet/anti_aim/movement_capture.hpp"
#include "skeet/anti_aim/targets.hpp"
#include "skeet/client/command_angle_retention.hpp"
#include "skeet/client/command_capture.hpp"
#include "skeet/client/command_capture_runtime_owner.hpp"
#include "skeet/client/command_recapture.hpp"
#include "skeet/client/command_transform.hpp"
#include "skeet/client/entity_command_owner.hpp"
#include "skeet/client/generated_command_owners.hpp"
#include "skeet/client/restricted_player_command.hpp"
#include "skeet/client/prediction_history_owner.hpp"
#include "skeet/movement/rage_stop.hpp"
#include "skeet/movement/simulation_environment.hpp"
#include "skeet/prediction/observer_registry.hpp"
#include "skeet/rage/targeting.hpp"
#include "skeet/client/runtime_storage.hpp"
#include "skeet/script/source_runtime_owner.hpp"
#include "skeet/script/lua_runtime_bindings.hpp"
#include <algorithm>
#include <bit>
#include <cstring>
#include <limits>
#include <immintrin.h>

namespace skeet::network {
namespace {
template <class T> T read(const void* object, std::ptrdiff_t offset) noexcept {
    T value;
    std::memcpy(&value, static_cast<const std::byte*>(object) + offset, sizeof value);
    return value;
}
template <class T> void write(void* object, std::ptrdiff_t offset, const T& value) noexcept {
    std::memcpy(static_cast<std::byte*>(object) + offset, &value, sizeof value);
}
std::int32_t add(std::int32_t a, std::int32_t b) noexcept {
    return std::bit_cast<std::int32_t>(std::uint32_t(a) + std::uint32_t(b));
}
std::int32_t subtract(std::int32_t a, std::int32_t b) noexcept {
    return std::bit_cast<std::int32_t>(std::uint32_t(a) - std::uint32_t(b));
}
std::uint32_t address(const void* object) noexcept {
    return std::uint32_t(reinterpret_cast<std::uintptr_t>(object));
}
float multiply(float a, float b) noexcept {
    return _mm_cvtss_f32(_mm_mul_ss(_mm_set_ss(a), _mm_set_ss(b)));
}
float sum(float a, float b) noexcept {
    return _mm_cvtss_f32(_mm_add_ss(_mm_set_ss(a), _mm_set_ss(b)));
}
MovementVector vector(const std::array<float, 3>& v) noexcept {
    return {v[0], v[1], v[2]};
}
MovementAngles angles(const std::array<float, 3>& a) noexcept {
    return {a[0], a[1], a[2]};
}
void* frame_weapon(const client::CommandFrame& frame) noexcept {
    return reinterpret_cast<void*>(std::uintptr_t(frame.weapon_address));
}
std::int32_t process_ticks(const TickShiftRuntimeSettings& settings, float interval) noexcept {
    auto time =
        _mm_div_ss(_mm_cvtepi32_ps(_mm_cvtsi32_si128(settings.process_milliseconds_43468290)),
                   _mm_set_ss(1000.0f));
    time = _mm_min_ss(_mm_max_ss(time, _mm_setzero_ps()), _mm_set_ss(1.0f));
    return _mm_cvttss_si32(_mm_add_ss(_mm_div_ss(time, _mm_set_ss(interval)), _mm_set_ss(0.5f)));
}
void apply_buttons(void* player, const client::Command& command,
                   const TickShiftPhase7Offsets& o) noexcept {
    const auto old = read<std::uint32_t>(player, o.buttons_4746d093);
    write(player, o.previous_buttons_e765ed03, old);
    write(player, o.buttons_4746d093, command.movement.buttons);
    const auto changed = read<std::uint32_t>(player, o.previous_buttons_e765ed03) ^
                         read<std::uint32_t>(player, o.buttons_4746d093);
    write(player, o.pressed_buttons_3db8eb39,
          changed & read<std::uint32_t>(player, o.buttons_4746d093));
    write(player, o.released_buttons_356b64e8,
          changed & ~read<std::uint32_t>(player, o.buttons_4746d093));
}
struct WeaponTime {
    float* target = nullptr;
    std::uint32_t saved = 0;
    WeaponTime(void* weapon, const client::CommandCaptureOffsets& o, float now) noexcept {
        if (!weapon)
            return;
        target =
            reinterpret_cast<float*>(static_cast<std::byte*>(weapon) + o.weapon_field_d2563274 - 8);
        std::memcpy(&saved, target, 4);
        std::memcpy(target, &now, 4);
    }
    void restore() noexcept {
        if (target) {
            std::memcpy(target, &saved, 4);
            target = nullptr;
        }
    }
};

// The two identical433B0D5C/CE0FE/D7606/849B5/CC0F2 sequences occur before
// cloning and after each generated movement/AA pass. Every tick reads current
// ConVars, player properties and collision interfaces through canonical bodies.
bool predictive_attack_clear(const client::CommandFrame& frame, void* player,
                             TickShiftRuntimeBindings& runtime, TickShiftPhase7Bindings& bindings) {
    if (!(frame.flags & 0x400U) || !frame.weapon_address || !(frame.flags & 0x40000U))
        return false;
    auto* weapon = frame_weapon(frame);
    if (!classified_packet_weapon(
            read<std::uint16_t>(weapon, runtime.firing.offsets.weapon_definition),
            runtime.weapon_classification))
        return false;
    if (!hotkey_active(bindings.predictive_attack_key, bindings.input) ||
        bindings.target.handle == -1)
        return false;
    auto* target =
        engine::invoke<void*>(bindings.capture.globals().entity_list, 4, bindings.target.handle);
    if (!target)
        return false;
    const auto speed = rage_stop_speed(weapon);
    const auto threshold = multiply(speed, speed);
    MovementSimulation simulation;
    const MovementCaptureInput movement{bindings.movement.offsets, bindings.movement.convars,
                                        *runtime.tick_interval, &bindings.movement_environment};
    initialize_live_movement_simulation(simulation, player, vector(frame.origin_10c),
                                        vector(frame.velocity_150), frame.player_flags_7c,
                                        std::uint32_t(add(frame.tick_base_c8, 1)), movement);
    for (std::int32_t i = 0; i < 14; ++i) {
        const auto square = sum(multiply(simulation.velocity.x, simulation.velocity.x),
                                multiply(simulation.velocity.y, simulation.velocity.y));
        if (threshold >= square)
            break;
        auto command = frame.previous_command;
        prepare_rage_stop_command(frame, player, command, simulation, bindings.stopping);
        simulate_live_movement_tick(simulation, bindings.collision, bindings.movement_environment,
                                    player, command.movement, angles(command.angles));
    }
    const auto eye = rage::target_eye_position(bindings.targeting, target);
    const MovementVector start{simulation.position.x, simulation.position.y,
                               sum(simulation.position.z, simulation.view_offset_z)};
    const auto ray = make_movement_hull_ray(start, {eye[0], eye[1], eye[2]}, {0, 0, 0}, {0, 0, 0});
    MovementCollisionTrace trace;
    trace.fraction = 1.0f;
    trace.all_solid = trace.start_solid = 0;
    trace.hitgroup = -1;
    trace.entity = 0;
    trace.hitbox = -1;
    trace_movement_world(bindings.collision, ray, 0x600400BU, trace, bindings.transform_override);
    return !(1.0f > trace.fraction) && !trace.all_solid && !trace.start_solid;
}

void capture_generated_frame(client::CommandFrame& frame, void* player, client::Command& command,
                             TickShiftRuntimeBindings& runtime, TickShiftPhase7Bindings& bindings) {
    const auto capture = bindings.capture.recapture(player);
    const auto& common = capture.offsets.common;
    const auto& o = bindings.capture_offsets;
    frame.angles_118 = command.angles;
    frame.input_side_80 = command.movement.side_move;
    frame.input_forward_78 = command.movement.forward_move;
    write(&frame, 0xF8, read<std::uint32_t>(player, common.player_flags));
    const auto tick = read<std::int32_t>(player, runtime.offsets.tick_base_3d10b94c);
    write(&frame, 0x88, tick);
    frame.tick_base_c8 = tick;
    const auto duck = read<float>(player, common.duck_amount);
    write(&frame, 0x1A4, duck);
    frame.duck_amount_98 = duck;
    write(&frame, 0x8C, read<std::array<float, 3>>(player, o.vector_48442721));
    const auto vy = read<float>(player, common.velocity + 4),
               vx = read<float>(player, common.velocity);
    frame.previous_speed_190 =
        _mm_cvtss_f32(_mm_sqrt_ss(_mm_set_ss(sum(multiply(vx, vx), multiply(vy, vy)))));
    frame.origin_168 = *engine::invoke<const std::array<float, 3>*>(player, 10);
    frame.previous_velocity_180 = read<std::array<float, 3>>(player, common.velocity);
    const auto handle = read<std::uint32_t>(player, capture.offsets.active_weapon);
    auto* weapon =
        handle == UINT32_MAX ? nullptr : engine::invoke<void*>(capture.entity_list, 4, handle);
    frame.weapon_address = address(weapon);
    if (weapon) {
        write(&frame, 0x194, read<float>(weapon, common.weapon_field_6777c9ce));
        write(&frame, 0xF4, read<float>(weapon, o.weapon_field_d2563274));
    }
    frame.flags &= ~0x400000U;
    write(&frame, 0x1B4, 0U);
    client::capture_command_physical_frame(frame, command, capture,
                                           client::CommandCapturePass::initial);
    frame.field_108 = frame.field_c4;
    frame.field_1d0 = read<std::uint32_t>(player, o.team);
    frame.flags |= 4U;
}

void run_generated_player(void* player, client::Command& command,
                          const client::CommandFrame& original, TickShiftPhase7Bindings& bindings,
                          float current_time) {
    WeaponTime weapon_time(frame_weapon(original), bindings.capture_offsets, current_time);
    if (bindings.prediction_bindings.flag_head)
        prediction::set_prediction_flags(*bindings.prediction_bindings.flag_head, 1);
    client::save_final_command(command);
    auto* copy = static_cast<std::byte*>(player) + bindings.offsets.command_copy_5df1a4bc;
    write(copy, 0, std::uint8_t(1));
    if (copy + 4 != reinterpret_cast<std::byte*>(&command))
        std::memcpy(copy + 4, &command, sizeof command);
    write(copy, 0x68, command.number);
    write(player, bindings.offsets.command_marker_a249061a, -1);
    client::CommandCopyOverrideScope guard(bindings.transform_override);
    engine::invoke<void>(player, 155);
    engine::invoke<const void*>(player, 10);
    guard.restore();
    if (bindings.prediction_bindings.flag_head)
        prediction::set_prediction_flags(*bindings.prediction_bindings.flag_head, 0);
    weapon_time.restore();
}
}

void process_tick_shift_phase7(TickShiftRuntimeState& runtime, TickShiftPhaseState& phase,
                            client::CommandFrame& frame, void* player, client::Command& command,
                            std::uint8_t& send, const TickShiftRuntimeSettings& settings,
                            TickShiftRuntimeBindings& live, TickShiftPhase7Bindings& bindings,
                            std::uint32_t& flags) {
    auto& state = runtime.network;
    if (!send || read<std::uint8_t>(player, live.offsets.player_block_9d920e35))
        return;
    if ((state.flags & 9U) != 9U) {
        if (state.mode == 2 && bindings.last_attack_command > *live.last_outgoing &&
            bindings.last_attack_command <= command.number)
            frame.flags |= 0x10000000U;
        return;
    }
    auto count =
        (std::min)(subtract(subtract(state.budget, *live.choked_commands), 1), state.available);
    for (std::int32_t i = 0; i < state.history_count; ++i)
        count = (std::min)(count, state.history[(state.history_first + std::uint32_t(i)) & 7U]);
    if (count <= 0)
        return;
    state.flags &= ~8U;
    const auto saved_tick = read<std::int32_t>(player, live.offsets.tick_base_3d10b94c);
    auto* const saved_time_target = live.current_time;
    const auto saved_time = *saved_time_target;
    const auto prediction =
        prediction::bind_command_prediction(bindings.prediction_bindings, live.prediction, player,
                                            command, bindings.prediction_player_offsets);
    auto targets = prediction.targets;
    lagcomp::PredictionScope outer(targets, address(player), address(&command),
                                   std::uint32_t(command.random_seed));
    if (std::uint32_t(command.tick) != 0x7F7FFFFFU) {
        if (state.last_command != *live.last_outgoing) {
            const auto process = process_ticks(settings, *live.tick_interval);
            const auto latency = average_latency_ticks(live.timing.latency);
            const auto adjusted = adjusted_command_tick(
                live.timing.command_tick, live.timing.have_ack, live.timing.acknowledged,
                live.timing.have_sent, live.timing.sent);
            const auto tick = add(add(add(subtract(process, count), 1), latency), adjusted);
            write(player, live.offsets.tick_base_3d10b94c, tick);
        }
        engine::invoke<void>(bindings.prediction_bindings.move_helper, 1, player);
        *live.current_time =
            multiply(float(read<std::int32_t>(player, live.offsets.tick_base_3d10b94c)),
                     *live.tick_interval);
        apply_buttons(player, command, bindings.offsets);
        if (auto* weapon = frame_weapon(frame)) {
            write(weapon, bindings.capture_offsets.weapon_field_d2563274,
                  read<float>(&frame, 0xF4));
            write(weapon, bindings.capture.physical_offsets().common.weapon_field_6777c9ce,
                  read<float>(&frame, 0x194));
        }
        WeaponTime weapon_time(frame_weapon(frame), bindings.capture_offsets, *live.current_time);
        engine::invoke<void>(player, 319);
        write(player, live.offsets.tick_base_3d10b94c,
              add(read<std::int32_t>(player, live.offsets.tick_base_3d10b94c), 1));
        engine::invoke<void>(bindings.prediction_bindings.move_helper, 1,
                             static_cast<void*>(nullptr));
        bindings.prediction_observers.finish_forward(player, command);
        weapon_time.restore();
    }
    bool initial_clear = false;
    if (command.movement.buttons & 1U) {
        bindings.attack_mask = ~frame.field_1dc & bindings.attack_policy;
        if (flags & 0x20000U)
            bindings.attack_mask &= ~4U;
    } else if (!(flags & 0x20000U) && bindings.predictive_attack)
        initial_clear = predictive_attack_clear(frame, player, live, bindings);
    bool clear = false;
    auto number = frame.previous_command.number;
    auto clone_choked = frame.choked_15c;
    for (std::int32_t i = 0; i < count; ++i) {
        const auto previous_clear = clear;
        clone_choked = add(clone_choked, 1);
        number = add(number, 1);
        const auto index = std::uint32_t(number) % 150U;
        auto& generated = live.commands[index];
        const bool last = i == subtract(count, 1);
        std::uint8_t generated_send = last;
        if (&generated != &frame.previous_command)
            generated = frame.previous_command;
        generated.number = number;
        generated.tick = command.tick;
        generated.movement.buttons |= command.movement.buttons & 0x400000U;
        generated.predicted = 1;
        *targets.player_current_command = address(&generated);
        generated.random_seed = std::int32_t(bindings.seed_generator(command.number) & 0x7FFFFFFFU);
        *bindings.prediction_seed = generated.random_seed;
        *live.current_time =
            multiply(float(read<std::int32_t>(player, live.offsets.tick_base_3d10b94c)),
                     *live.tick_interval);
        client::CommandFrame cloned;
        client::initialize_command_frame(cloned);
        cloned = frame;
        cloned.previous_command.number = generated.number;
        cloned.previous_command.tick = generated.tick;
        cloned.command_number_ac = generated.number;
        cloned.send_packet_address = address(&generated_send);
        cloned.choked_15c = clone_choked;
        capture_generated_frame(cloned, player, generated, live, bindings);
        if (last)
            cloned.choke_limit_fc = clone_choked;
        auto& script_data = bindings.scripts.storage().data();
        auto environment = bindings.angle_environment;
        environment.shared_player = script_data.native_cell<void**>(0x4346A3A4u);
        const client::ScriptCommandBindings script{
            &bindings.scripts.bridge(),
            script_data.native_cell<client::ScriptCommand*>(0x43479300u),
            "setup_command",
            script_data.native_cell<const std::int32_t*>(0x434757DCu),
            &script_data.native_cell<std::uint8_t>(0x43475A5Cu),
            script::source_lua_runtime_bindings().lua.pushlightuserdata,
            environment};
        client::dispatch_command_script(script, generated, cloned.angles_118[1], generated_send,
                                        flags);
        client::process_generated_movement_command(bindings.owners, cloned, player, generated,
                                                   generated_send);
        const auto saved_limit = cloned.choke_limit_fc;
        cloned.choke_limit_fc = clone_choked;
        const auto requested_send = generated_send;
        generated_send = 1;
        client::process_generated_angle_command(bindings.owners, cloned, player, generated,
                                                generated_send);
        generated_send = requested_send;
        cloned.choke_limit_fc = saved_limit;
        if (!(flags & 0x20000U)) {
            if (command.movement.buttons & 3U)
                clear = previous_clear;
            else if (!clear && !initial_clear && bindings.predictive_attack)
                clear = predictive_attack_clear(cloned, player, live, bindings);
            if (clear) {
                flags |= 8U;
                bindings.attack_mask |= 4U;
            }
        }
        const client::CommandButtonPolicy policy{bindings.inverted_commands,
                                                 bindings.anti_aim_other.leg_movement.value,
                                                 *live.choked_commands};
        client::reorient_command_movement(generated, cloned.angles_118, cloned.state_104,
                                          (cloned.flags & 0x40000U) != 0, !last, policy,
                                          bindings.trigonometry);
        environment.shared_player = script_data.native_cell<void**>(0x4346A3A4u);
        client::sanitize_command(generated, (cloned.flags & 8U) != 0, environment);
        run_generated_player(player, generated, frame, bindings, *live.current_time);
        write(player, live.offsets.tick_base_3d10b94c,
              subtract(read<std::int32_t>(player, live.offsets.tick_base_3d10b94c), 1));
        generated.predicted = 0;
        client::save_final_command(generated);
        auto& verified = live.verified_commands[index];
        if (&verified.command != &generated)
            verified.command = generated;
        verified.checksum = live.checksum ? live.checksum(&generated) : 0;
        if (auto* channel = engine::invoke<void*>(live.engine_client, 78))
            engine::invoke<void>(channel, 45);
        *live.choked_commands = add(*live.choked_commands, 1);
        generated_send = last;
        client::process_restricted_player_command(
            client::prediction_history_shared(bindings.restricted_owner), player, &generated,
            &generated_send, false, bindings.restricted);
        write(player, live.offsets.tick_base_3d10b94c,
              add(read<std::int32_t>(player, live.offsets.tick_base_3d10b94c), 1));
    }
    bindings.attack_mask = UINT32_MAX;
    notify_tick_shift_generated_observers(bindings.generated_observers, player, number, count);
    state.available = state.mode ? (std::max)(subtract(state.available, count), 0) : 0;
    if (auto* const history = live.prediction_history_owner)
        history->state.prediction_stamp = *live.history_stamp_4346a334;
    bindings.next_generated_command = add(number, 1);
    outer.restore();
    *saved_time_target = saved_time;
    write(player, live.offsets.tick_base_3d10b94c, saved_tick);
    send = 1;
    state.history_count = 0;
    state.history_first = 0;
    phase.recharge_ready_command_11c = (std::numeric_limits<std::int32_t>::max)();
    flags = (flags & ~0x400U) | 0x200U;
}
} //namespace skeet::network
