#include "skeet/network/packet_control.hpp"
#include <algorithm>
#include <bit>
#include <cmath>
#include <immintrin.h>

namespace skeet::network {
namespace {
std::int32_t signed_bits(std::uint32_t value) noexcept {
    return std::bit_cast<std::int32_t>(value);
}
std::int32_t add(std::int32_t a, std::int32_t b) noexcept {
    return signed_bits(std::uint32_t(a) + std::uint32_t(b));
}
std::int32_t sub(std::int32_t a, std::int32_t b) noexcept {
    return signed_bits(std::uint32_t(a) - std::uint32_t(b));
}
std::int32_t absolute(std::int32_t value) noexcept {
    return value < 0 ? sub(0, value) : value;
}
float absolute(float value) noexcept {
    return std::bit_cast<float>(std::bit_cast<std::uint32_t>(value) & 0x7fffffffU);
}
std::int32_t truncate(float value) noexcept {
    return _mm_cvttss_si32(_mm_set_ss(value));
}
void force_send(std::uint32_t& flags) noexcept {
    flags = (flags & ~0x400U) | 0x200U;
}
}

void reset_fake_lag(FakeLagState& state) noexcept {
    state.tracked_target = -1;
    state.last_origin = {};
    state.saved_duck_amount_bits = 0;
    state.previous_speed_delta = 0;
    state.value_7c = -1;
    state.value_8c = -1;
    state.flags = 0;
    state.target = 0;
    state.recorded_choked = 0;
    state.value_58 = 0;
    state.previous_selected = 0;
}
void prepare_fake_lag_command(FakeLagState& state, const PacketCommandContext& context,
                              std::uint32_t& flags) noexcept {
    if (context.force_on_next_command && context.engine_choked == 0 &&
        context.command_number == add(context.last_command, 1))
        force_send(flags);
    if (context.engine_choked == 0) {
        state.previous_selected = state.recorded_choked;
        state.last_origin = context.player_origin;
        state.saved_duck_amount_bits = context.player_3309c0cf;
    }
    state.recorded_choked = context.engine_choked;
}
void begin_fake_lag_tick(FakeLagState& state, const PacketFrame& frame,
                         bool& trace_result) noexcept {
    trace_result = false;
    if (frame.choked == 0) {
        state.cycle = add(state.cycle, 1);
        state.target = 0;
    }
    state.acceleration = {frame.velocity.x - frame.previous_velocity.x,
                          frame.velocity.y - frame.previous_velocity.y, 0.0F};
}

bool select_fake_lag_limit(FakeLagState& state, const PacketFrame& frame, float interval,
                           const RandomInterface& random) {
    if (state.mode == 2) {
        state.target = frame.limit;
        return false;
    }
    const auto product = signed_bits(std::uint32_t(state.variance) * std::uint32_t(frame.limit));
    const auto variance = add(product, 99) / 100;
    auto upper = frame.limit;
    auto selected = std::max(0, sub(upper, variance));
    if (state.mode == 3 && (std::uint32_t(state.cycle) & 1U)) {
        if (variance == 0)
            selected = 0;
        else {
            random.seed(random.context, state.cycle);
            // The native arithmetic shift rounds negative results down.
            selected = random.integer(random.context, 0, variance) >> 1;
        }
    } else {
        if (state.mode == 0 &&
            frame.velocity.x * frame.velocity.x + frame.velocity.y * frame.velocity.y > 4.0F) {
            float vx = frame.velocity.x, vy = frame.velocity.y;
            const float ax = (vx - frame.previous_velocity.x) * 0.5F;
            const float ay = (vy - frame.previous_velocity.y) * 0.5F;
            float x = frame.origin.x, y = frame.origin.y;
            auto step = frame.choked;
            if (step <= upper)
                do {
                    const auto next = add(step, 1);
                    const float dx = x - state.last_origin.x;
                    const float dy = y - state.last_origin.y;
                    if (dy * dy + dx * dx > 4096.0F) {
                        upper = std::min(next, upper);
                        selected = step;
                        break;
                    }
                    vx = vx + ax;
                    vy = vy + ay;
                    x = x + vx * interval;
                    y = y + vy * interval;
                    step = next;
                } while (step <= upper);
        }
        if (selected != upper) {
            random.seed(random.context, state.cycle);
            selected = random.integer(random.context, selected, upper);
            if (selected == state.previous_selected) {
                if (selected < frame.limit &&
                    (selected <= 0 || state.mode == 0 || random.integer(random.context, 0, 1) != 0))
                    selected = add(selected, 1);
                else
                    selected = sub(selected, 1);
            }
        }
    }
    state.selected = selected;
    return true;
}

void apply_fake_lag_send_gate(const FakeLagState& state, const PacketFrame& frame,
                              std::uint32_t flags, std::uint8_t& send_packet) noexcept {
    if (!(flags & 0x600U) && frame.choked < frame.limit && frame.choked < state.target)
        send_packet = 0;
}
bool fake_lag_pending(std::int32_t configured_limit, const PacketFrame& frame,
                      std::uint32_t flags) noexcept {
    return !(flags & 0x600U) && frame.choked < frame.limit &&
           (configured_limit < 1 || frame.choked < configured_limit);
}
bool velocity_trigger(const FakeLagState& state, const PacketFrame& frame) noexcept {
    const auto& v = frame.velocity;
    const auto& p = frame.previous_velocity;
    const float current =
        1.0F / (std::sqrt((v.x * v.x + v.y * v.y) + v.z * v.z) + 1.1920928955078125e-7F);
    const float previous =
        1.0F / (std::sqrt((p.x * p.x + p.y * p.y) + p.z * p.z) + 1.1920928955078125e-7F);
    const float dot = ((p.y * previous) * (v.y * current) + (p.x * previous) * (v.x * current)) +
                      (p.z * previous) * (v.z * current);
    if (!(dot >= 0.9998477101325989F))
        return true;
    return absolute((std::sqrt(v.x * v.x + v.y * v.y) - frame.previous_speed) -
                    state.previous_speed_delta) > 2.0F;
}
bool displacement_trigger(const FakeLagState& state, const PacketFrame& frame, float speed,
                          std::uint32_t buttons, float interval, float gravity) noexcept {
    if (!(speed >= 80.0F) || (frame.limit <= 7 && !(frame.flags & 0x4000U)) ||
        ((frame.flags & 0x40000U) && !(buttons & 2U)))
        return false;
    float vx = frame.velocity.x, vy = frame.velocity.y, vz = frame.velocity.z;
    float x = frame.origin.x, y = frame.origin.y, z = frame.origin.z;
    const float half_gravity = (interval * gravity) * 0.5F;
    auto step = add(frame.choked, 1);
    if (step <= frame.limit)
        do {
            vx += state.acceleration.x;
            vy += state.acceleration.y;
            const float intermediate_z = (vz + state.acceleration.z) - half_gravity;
            x = vx * interval + x;
            y = vy * interval + y;
            vz = intermediate_z - half_gravity;
            z = z + intermediate_z * interval;
            const float dx = x - state.last_origin.x, dy = y - state.last_origin.y,
                        dz = z - state.last_origin.z;
            if ((dy * dy + dx * dx) + dz * dz > 4096.0F)
                return true;
            step = add(step, 1);
        } while (step <= frame.limit);
    return false;
}
void finish_fake_lag_tick(FakeLagState& state, const PacketFrame& frame, std::int32_t target,
                          std::int32_t target_state) noexcept {
    state.previous_speed_delta =
        std::sqrt(frame.velocity.x * frame.velocity.x + frame.velocity.y * frame.velocity.y) -
        frame.previous_speed;
    state.tracked_target = target;
    state.flags = (state.flags & ~2U) | (target != -1 && target_state != 0 ? 2U : 0U);
}
bool trigger_target_change(LagTriggerState& state, float speed, std::int32_t target,
                           std::int32_t target_state, float time, std::uint32_t& flags) noexcept {
    if (!(speed > 2.0F) || target == -1 || target != state.target || (state.flags & 2U) ||
        target_state == 0 || !(absolute(state.last_time - time) > 0.2F))
        return false;
    force_send(flags);
    state.flags |= 4U;
    state.last_time = time;
    return true;
}
std::int32_t average_latency_ticks(const LatencySamples& samples) noexcept {
    if (samples.count <= 0)
        return 1;
    float sum = 0;
    for (std::int32_t i = 0; i < samples.count; ++i)
        sum += samples.values[(samples.first + std::uint32_t(i)) & 15U];
    return _mm_cvtss_si32(_mm_set_ss(sum / static_cast<float>(samples.count)));
}
std::int32_t adjusted_command_tick(std::int32_t base, bool have_ack, std::int32_t ack,
                                   bool have_sent, std::int32_t sent) noexcept {
    const auto delta = sub(sent, ack);
    if (have_ack && have_sent && std::uint32_t(sub(delta, 1)) < 9U)
        return add(base, delta);
    return base;
}
std::int32_t network_command_budget(bool have_rules, bool rule_0, bool rule_1, std::int32_t mode,
                                    std::int32_t configured) noexcept {
    if (have_rules) {
        if (!rule_0)
            return configured;
        if (rule_1 && mode == 1)
            return 3;
    }
    return 7;
}
std::int32_t next_shuffle_random(ShuffleRandom& state) noexcept {
    auto step = [&state]() {
        const auto quotient = state.seed / 127773;
        state.seed = signed_bits(std::uint32_t(state.seed) * 16807U +
                                 std::uint32_t(quotient) * std::uint32_t(-2147483647));
        if (state.seed < 0)
            state.seed = add(state.seed, 2147483647);
    };
    if (state.seed < 1 || state.previous == 0) {
        state.seed = sub(0, state.seed);
        if (state.seed < 1)
            state.seed = 1;
        for (std::int32_t j = 39; j >= 0; --j) {
            step();
            if (j < 32)
                state.table[std::uint32_t(j)] = state.seed;
        }
        state.previous = state.table[0];
    }
    step();
    // x86 computes signed division by 2^26, with its correction for negatives.
    auto index = state.previous / 67108864;
    if (index < 0 || index > 31)
        index = signed_bits(std::uint32_t(index) & 31U);
    state.previous = state.table[std::uint32_t(index)];
    state.table[std::uint32_t(index)] = state.seed;
    return state.previous;
}
std::int32_t pending_tick_shift_commands(const TickShiftState& state, std::int32_t choked) noexcept {
    auto result = std::min(sub(sub(state.budget, choked), 1), state.available);
    for (std::int32_t i = 0; i < state.history_count; ++i)
        result = std::min(result, state.history[(state.history_first + std::uint32_t(i)) & 7U]);
    return add(add(choked, 1), result);
}
void select_tick_shift_mode(TickShiftState& state, std::int32_t budget,
                         const TickShiftGates& gates) noexcept {
    state.mode = 0;
    state.budget = budget;
    state.minimum = sub(budget, 1);
    if (!gates.connected || !gates.checksum_available || budget < 7 ||
        (gates.freeze_period && gates.freeze_entity_present && gates.freeze_entity_state > 1) ||
        gates.player_blocked || gates.command_blocked)
        return;
    if (gates.fake_peek_active)
        state.mode = 3;
    else if (gates.double_tap_active)
        state.mode = 1;
    else if (gates.on_shot_active)
        state.mode = 2;
    else
        return;
    state.minimum = std::max(1, state.fake_lag_limit);
    state.limit = std::clamp(sub(sub(budget, state.fake_lag_limit), 1), 1, 15);
    if (state.mode == 2)
        state.flags &= ~1U;
}
std::int32_t cap_fake_lag_commands(const FakeLagState& state, std::int32_t limit) noexcept {
    return std::min(state.configured_cap, limit);
}
std::int32_t cap_tick_shift_commands(const TickShiftState& state, std::int32_t limit) noexcept {
    return state.mode != 0 || state.available > 0 ? std::min(state.minimum, limit) : limit;
}
void update_tick_shift_command(TickShiftState& state, TickShiftFrame& frame, TickShiftCommand& command,
                            const TickShiftTiming& timing, std::uint32_t& flags) noexcept {
    frame.choke_end = add(add(timing.choked, state.available), 1);
    frame.tick_shift_mode = state.available < state.limit ? 0 : state.mode;
    frame.next_tick = frame.tick_base;
    frame.saved_tick_base = frame.tick_base;
    frame.flags = (frame.flags & ~0x40000000U) | ((state.flags << 25) & 0x40000000U);
    if (absolute(sub(frame.tick, state.shot_command)) < 10)
        frame.flags |= 0x08000000U;
    if (state.mode != 0) {
        float process = static_cast<float>(timing.process_milliseconds) / 1000.0F;
        // Native MAXSS/MINSS return the second operand for unordered comparisons.
        if (!(process > 0.0F))
            process = 0.0F;
        if (!(process < 1.0F))
            process = 1.0F;
        const auto latency = average_latency_ticks(timing.latency);
        const auto tick = adjusted_command_tick(timing.command_tick, timing.have_ack,
                                                timing.acknowledged, timing.have_sent, timing.sent);
        const auto process_ticks = truncate(process / timing.interval + 0.5F);
        const auto base = add(latency, tick);
        frame.next_tick = add(base, process_ticks);
        if (state.available >= state.limit && frame.has_weapon) {
            const float upper =
                static_cast<float>(sub(add(base, process_ticks), 1)) * timing.interval;
            const float lower = static_cast<float>(add(add(add(tick, 1), latency),
                                                       sub(process_ticks, state.limit))) *
                                timing.interval;
            if (upper >= timing.player_next_attack && lower < timing.player_next_attack &&
                upper >= timing.weapon_next_attack)
                frame.flags |= 0x04000000U;
        }
        if (state.mode == 2 && state.available >= state.limit && !(frame.flags & 0x20000000U) &&
            frame.weapon_id != 0x31 && frame.weapon_id != 0x40 && frame.weapon_type != 9) {
            state.flags |= 0x10U;
            const auto now = truncate(timing.current_time / timing.interval + 0.5F);
            if (add(now, 2) > state.shot_tick || sub(state.shot_tick, now) >= 16 ||
                state.last_command != timing.last_outgoing || !(state.flags & 2U)) {
                force_send(flags);
                if (frame.movement_flags & 1U)
                    command.buttons &= ~1U;
            }
        }
        if (state.mode == 3 && state.available >= state.limit && !timing.freeze_period) {
            if (!(state.flags & 0x20U) && !(frame.flags & 0x20000000U)) {
                state.flags |= 0x10U;
                force_send(flags);
            } else {
                command.pitch = 89.0F;
                state.random.seed = sub(0, absolute(command.number));
                state.random.previous = 0;
                float value = static_cast<float>(
                    static_cast<double>(next_shuffle_random(state.random)) * 4.656612875245797e-10);
                if (static_cast<double>(value) > 0.99999988)
                    value = 0.9999998807907104F;
                command.yaw = (value + 0.0F) * 360.0F - 180.0F;
            }
        }
    }
    if (frame.tick_shift_mode == 2 &&
        (state.available < state.limit || (frame.flags & 0x20000000U) || frame.weapon_id == 0x31 ||
         frame.weapon_id == 0x40 || frame.weapon_type == 9))
        frame.tick_shift_mode = 0;
}
void apply_tick_shift_weapon_gate(const TickShiftState& state, std::int32_t last_outgoing, bool enabled,
                               std::uint8_t weapon_type, std::int32_t choked,
                               std::uint32_t& frame_flags, std::uint32_t& command_flags,
                               std::uint8_t& send_packet) noexcept {
    if (state.mode != 0 && state.limit <= state.available && weapon_type == 9 && enabled) {
        frame_flags |= 1U;
        force_send(command_flags);
        if (state.last_command == last_outgoing)
            frame_flags |= 2U;
        if (choked == 0 && send_packet != 0)
            send_packet = 0;
    }
}
} // namespace skeet::network
