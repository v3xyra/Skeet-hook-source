#include "skeet/rage/preamble.hpp"
#include "skeet/rage/acquisition.hpp"
#include "skeet/client/target_refresh_runtime_owner.hpp"
#include "skeet/engine/interface_call.hpp"
#include "skeet/lagcomp/update_timing.hpp"
#include <bit>
#include <cstring>
#include <immintrin.h>

namespace skeet::rage {
namespace {
template <class T> T field(const void* object, std::ptrdiff_t offset) noexcept {
    T result;
    std::memcpy(&result, static_cast<const std::byte*>(object) + offset, sizeof result);
    return result;
}
template <class T> void store(void* object, std::ptrdiff_t offset, const T& value) noexcept {
    std::memcpy(static_cast<std::byte*>(object) + offset, &value, sizeof value);
}
void* weapon_of(const client::CommandFrame& frame) noexcept {
    return reinterpret_cast<void*>(std::uintptr_t(frame.weapon_address));
}
bool convar_nonzero(const void* object) noexcept {
    for (;;) {
        const auto parent = field<const void*>(object, 0x1C);
        if (parent == object)
            break;
        object = parent;
    }
    return field<std::uint32_t>(object, 0x30) !=
           std::uint32_t(reinterpret_cast<std::uintptr_t>(object));
}
std::int32_t sub(std::int32_t first, std::int32_t second) noexcept {
    return std::bit_cast<std::int32_t>(std::uint32_t(first) - std::uint32_t(second));
}
std::int32_t dt_hit_chance(const PreambleBindings& b) noexcept {
    return b.settings.double_tap_hit_chance.value > 0
               ? b.settings.double_tap_hit_chance.value
               : b.weapon_settings.minimum_hit_chance.value;
}
std::uint32_t hash_command(const client::Command& command) noexcept {
    std::uint32_t result = 0x811C9DC5;
    for (const auto byte : std::as_bytes(std::span(&command, 1)))
        result = (std::to_integer<std::uint8_t>(byte) ^ result) * 0x1000193;
    return result ^ 0x92CA5DF1;
}
void refresh_target(const PreambleBindings& bindings, const client::CommandFrame& original,
                    const client::Command& command, void* player) {
    if (auto* live = std::get_if<std::reference_wrapper<const client::TargetRefreshRuntimeOwner>>(
            &bindings.target_refresh)) {
        live->get().refresh(bindings.yaw_target, player, command,
                            original.previous_command.movement.buttons,
                            field<float>(&original, 0x11C));
        return;
    }
    const auto& refresh = std::get<std::reference_wrapper<const anti_aim::TargetRefreshContext>>(
                              bindings.target_refresh)
                              .get();
    const auto& base = refresh.scored;
    const anti_aim::TargetScoredContext scored{
        {base.common, command, original.previous_command.movement.buttons,
         field<float>(&original, 0x11C), base.player_rules, base.dormant_health, base.offsets,
         base.geometry_offsets, base.local_pose_origin, base.local_pose, base.trigonometry,
         base.sequence, base.clock_owner, base.local_time_offset, base.trace_filter_vtable,
         base.trace_entities},
        base.movement,
        base.movement_environment};
    anti_aim::refresh_yaw_target(bindings.yaw_target, {scored, refresh.selector_convar});
}
struct ScanTemplate {
    std::uint32_t mode;
    RageHitboxMasks hitboxes;
    float scale;
    std::uint32_t multipoint;
    std::int32_t damage;
    float chance, minimum_cosine;
    std::array<std::uint8_t, 8> options;
};
static_assert(sizeof(ScanTemplate) == 0x28);
PreambleScanTemplate prepare_template(const PreambleState& state, const client::CommandFrame& frame,
                                      const PreambleBindings& b) {
    const auto& g = b.settings;
    const auto& w = b.weapon_settings;
    ScanTemplate result{};
    result.mode = std::uint32_t(w.accuracy_boost.value ? w.accuracy_boost.value : 1);
    result.hitboxes = prepare_rage_hitboxes(
        std::uint32_t(g.target_hitboxes.value), g.prefer_safe_point.value != 0,
        hotkey_active(g.force_safe_point.value.binding, b.input),
        std::uint32_t(g.avoid_unsafe_hitboxes.value), state.flags, g.prefer_body_aim.value != 0,
        std::uint8_t(g.body_aim_disablers.value));
    result.scale = w.multipoint_scale.value == 24
                       ? 1.0f
                       : minimum_number(0.9f, float(w.multipoint_scale.value) * 0.01f - 0.01f);
    result.multipoint = hotkey_active(g.multipoint_key.value.binding, b.input)
                            ? std::uint32_t(w.multipoint.value)
                            : 0;
    result.damage = w.minimum_damage.value;
    result.chance = float((state.flags & 0x20U) ? dt_hit_chance(b) : w.minimum_hit_chance.value) *
                    0.01f;
    result.minimum_cosine =
        b.trigonometry.cosine(float(w.maximum_fov.value) * 0.01745329238474369049f);
    result.options[0] = g.prefer_safe_point.value;
    result.options[1] = g.prefer_body_aim.value;
    result.options[2] = w.delay_shot.value;
    result.options[3] = w.force_body_aim_on_peek.value;
    if (frame.weapon_id_d8 == 31) {
        if (result.damage < 105)
            result.damage = 105;
        result.options[1] = 1;
        if (field<float>(weapon_of(frame), b.offsets.taser_timing) > 0.05f)
            result.multipoint = 0;
    }
    if (frame.field_1a0 & 1U)
        result.mode = 1;
    if (frame.field_1a0 & 2U)
        result.multipoint &= ~0x200U;
    if (frame.field_1a0 & 0x10U)
        result.hitboxes.regular &= ~0x200U;
    if (frame.field_1a0 & 8U)
        result.multipoint &= ~0xC0U;
    if (frame.field_1a0 & 4U)
        result.multipoint &= ~0x30U;
    if ((state.flags & 0x400U) && frame.weapon_id_d8 != 31) {
        result.hitboxes.regular &= 2;
        result.hitboxes.safe &= 2;
        result.multipoint &= 2;
        result.options[1] = 0;
        result.options[3] = 0;
    }
    if (w.damage_override.value && hotkey_active(w.damage_override_key.value.binding, b.input)) {
        result.options[4] = 1;
        result.damage = w.override_damage.value;
        result.options[2] = 0;
    }
    return std::bit_cast<PreambleScanTemplate>(result);
}
void prepare_firing_branch(PreambleResult& out, const PreambleBindings& b, void* player) {
    auto& frame = *out.frame;
    auto& branch = out.firing;
    branch.field_22 = frame.weapon_id_d8 == 64 && (frame.flags & 0x40U);
    branch.alternate_path = false;
    branch.field_33 = false;
    branch.field_83 = false;
    branch.timing_ready = false;
    if (b.command_sequence_gate && frame.choked_15c != 0 &&
        b.selected_marker > b.current_sequence && b.selected_marker < frame.command_number_ac)
        return;
    branch.timing_ready =
        (frame.flags & 0x800U) ? (frame.flags & 2U) != 0 : (frame.flags & 0x400U) != 0;
    if (frame.field_1d4 == 2 && !(frame.flags & 0x40000000U)) {
        branch.timing_ready = false;
        if (frame.weapon_id_d8 != 64 && (frame.flags & 0x4000000U))
            branch.field_33 = true;
        return;
    }
    if (branch.timing_ready)
        return;
    branch.field_33 = frame.weapon_id_d8 != 64 && (frame.flags & 0x4000000U);
    if (!b.settings.silent_aim.value)
        return;
    const float attack =
        maximum_number(field<float>(player, b.offsets.player_next_attack),
                       field<float>(weapon_of(frame), b.offsets.weapon_next_attack));
    const float quotient = attack / b.interval;
    const auto rounded = _mm_cvtss_si32(_mm_set_ss(quotient));
    const std::uint32_t ceiling = std::uint32_t(rounded) + std::uint32_t(quotient > float(rounded));
    const auto current = _mm_cvttss_si32(_mm_set_ss(b.prediction_time / b.interval + 0.5f));
    branch.field_83 = sub(std::bit_cast<std::int32_t>(ceiling), current) == 1;
}
}
PreambleScanTemplate prepare_rage_scan_template(const PreambleState& state,
                                                const client::CommandFrame& frame,
                                                const PreambleBindings& bindings) {
    return prepare_template(state, frame, bindings);
}
PreambleResult prepare_rage_command(PreambleState& state, client::CommandFrame& original,
                                    client::Command& command, void* player,
                                    AcquisitionStorage& storage, const PreambleBindings& b) {
    PreambleResult out;
    out.frame = &original;
    out.command = &command;
    const auto previous = b.scheduler_flags;
    b.scheduler_flags = ((previous & 8U) << 11) | (previous & 0xFFFEAFE7U);
    auto* weapon = weapon_of(original);
    if (weapon && !field<std::uint8_t>(weapon, b.offsets.weapon_reload_guard)) {
        bool keep = false;
        if (command.movement.buttons & 0x2000U) {
            const auto ammunition = field<std::int32_t>(weapon, b.offsets.ammunition);
            if (ammunition != -1) {
                // Real weapon virtual+5DC, reached through the client ABI thunk.
                const auto maximum = engine::invoke<std::int32_t>(weapon, 375);
                keep = ammunition < maximum &&
                       field<std::int32_t>(weapon, b.offsets.reserve_ammunition) > 0;
            }
        }
        if (!keep)
            b.scheduler_flags &= ~0x100U;
    }
    state.flags = (original.flags & 0x8000000U) ? 0x20U : 0;
    if (original.flags & 0x4000U)
        state.flags |= 4;
    else if (((state.flags & 0x20U) ? dt_hit_chance(b)
                                    : b.weapon_settings.minimum_hit_chance.value) > 0)
        state.flags |= 0x10;
    if (original.flags & 8U)
        state.flags |= 0x200;
    if (convar_nonzero(b.restricted_convar))
        state.flags |= 0x400;
    if (!weapon)
        return out;
    bool active =
        b.settings.enabled.value && hotkey_active(b.settings.activation.value.binding, b.input);
    if (active) {
        active = !(b.game_rules && *b.game_rules &&
                   field<std::uint8_t>(*b.game_rules, b.offsets.local_guard));
        if (active)
            active = lagcomp::history_weapon_is_firearm(
                field<std::uint16_t>(weapon, b.offsets.weapon_identifier));
    }
    if (active && (b.scheduler_flags & 0x40U)) {
        if (original.choked_15c < original.choke_limit_fc || original.choked_15c <= 0)
            active = false;
        else {
            const auto count =
                state.frame_count < original.choked_15c ? state.frame_count : original.choked_15c;
            auto number = std::uint32_t(original.command_number_ac) - 1;
            for (std::int32_t i = 0; i < count; ++i, --number) {
                auto& candidate_command = b.command_ring[number % 150U];
                if (std::uint32_t(candidate_command.number) != number ||
                    std::uint32_t(candidate_command.tick) == 0x7F7FFFFFU)
                    continue;
                auto& candidate = state.frames[(state.frame_first + std::uint32_t(i)) & 15U];
                if (std::uint32_t(candidate.command_number_ac) == number &&
                    field<float>(&candidate, 0x84) > field<float>(out.frame, 0x84)) {
                    out.frame = &candidate;
                    out.command = &candidate_command;
                }
            }
            if (out.frame != &original) {
                auto& selected = *out.frame;
                out.prediction_clock_marker = 0;
                const auto number = std::uint32_t(selected.command_number_ac);
                if (const auto* saved_address = b.animation_history.at(number)) {
                    PreambleAnimationSnapshot saved;
                    std::memcpy(&saved, saved_address, sizeof saved);
                    if (saved.sequence == number) {
                        out.prediction_clock_marker =
                            std::uint32_t(reinterpret_cast<std::uintptr_t>(saved_address));
                        const auto* layers =
                            static_cast<std::byte*>(player) + b.offsets.animation_layers;
                        lagcomp::restore_animation(
                            saved.animation, field<void*>(layers, 0),
                            field<std::int32_t>(layers, 0xC),
                            reinterpret_cast<float*>(static_cast<std::byte*>(player) +
                                                     b.offsets.pose_parameters));
                        if (b.set_absolute_angles) {
                            const lagcomp::Vector angles{0.0f, saved.absolute_yaw, 0.0f};
                            b.set_absolute_angles(player, angles);
                        }
                    }
                }
                store(&selected, 0xD4, field<std::uint32_t>(&original, 0xD4));
                selected.field_18c = 0;
                store(&selected, 0x144, field<std::uint32_t>(&original, 0x144));
                out.previous_prediction_time = b.prediction_time;
                out.replaced_frame = true;
                b.prediction_time = float(field<std::int32_t>(&selected, 0xC8)) * b.interval;
                out.command_hash = hash_command(*out.command);
                state.flags |= 2;
            }
        }
    }
    if (b.settings.enabled.value)
        refresh_target(b, original, command, player);
    if (!active) {
        out.continuation = PreambleContinuation::inactive;
        return out;
    }
    auto& frame = *out.frame;
    out.tick_base = field<std::int32_t>(&frame, 0xC8);
    weapon = weapon_of(frame);
    out.point_scale_is_automatic = b.weapon_settings.multipoint_scale.value == 24;
    if (field<std::int32_t>(weapon, b.offsets.ammunition) == 0) {
        if (field<std::int32_t>(weapon, b.offsets.reserve_ammunition) > 0)
            out.command->movement.buttons = (out.command->movement.buttons & ~1U) | 0x2000U;
        out.continuation = PreambleContinuation::ammunition;
        return out;
    }
    if (*reinterpret_cast<const std::uint8_t*>(std::uintptr_t(frame.send_packet_address)) &&
        firing_mode(b.firing_mode_enabled, b.send_mode) == 1 && std::int32_t(frame.field_18c) < 1)
        frame.field_18c = 1;
    out.original_firing_bit = (frame.flags & 0x400U) != 0;
    prepare_firing_branch(out, b, player);
    out.scan_template = prepare_rage_scan_template(state, frame, b);
    const SpreadParameters spread{std::uint32_t(frame.weapon_id_d8),
                                  std::int32_t(frame.field_b0),
                                  field<float>(&frame, 0xD0),
                                  field<float>(&frame, 0xC4),
                                  std::bit_cast<float>(frame.field_100),
                                  field<std::int32_t>(&frame, 0x19C),
                                  b.shotgun_patterns,
                                  frame.weapon_kind_124};
    prepare_spread_cache(state.spread, spread, std::bit_cast<float>(frame.field_160),
                         std::uint32_t(frame.command_number_ac), (state.flags & 0x10U) != 0,
                         b.spread_tables, b.random, b.pattern, b.trigonometry);
    storage.queued.clear();
    state.completed.fill(0);
    state.evaluation_count = 0;
    out.continuation = b.weapon_settings.target_selection.value == 1
                           ? PreambleContinuation::single_target
                           : PreambleContinuation::multiple_targets;
    return out;
}
} // namespace skeet::rage
