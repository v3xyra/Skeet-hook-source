#include "skeet/network/tick_shift_acknowledgement.hpp"
#include "skeet/engine/interface_call.hpp"
#include <bit>
#include <cstring>
#include <immintrin.h>
#include <limits>

namespace skeet::network {
namespace {
template <class T> T field(const void* object, std::uint32_t offset) noexcept {
    T value;
    std::memcpy(&value, static_cast<const std::byte*>(object) + offset, sizeof value);
    return value;
}
std::int32_t add(std::int32_t a, std::int32_t b) noexcept {
    return std::bit_cast<std::int32_t>(std::uint32_t(a) + std::uint32_t(b));
}
std::int32_t subtract(std::int32_t a, std::int32_t b) noexcept {
    return std::bit_cast<std::int32_t>(std::uint32_t(a) - std::uint32_t(b));
}
bool acknowledgement_enabled(const void* object) noexcept {
    for (;;) {
        const auto* parent = field<const void*>(object, 0x1C);
        if (parent == object)
            break;
        object = parent;
    }
    const auto bits = field<std::uint32_t>(object, 0x2C) ^
                      static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(object));
    return std::bit_cast<float>(bits) != 0.0f; //43407DE2 admits unordered values
}
long double current_engine_time(void* engine_client) {
    return engine::invoke<long double>(engine_client, 14);
}
std::int32_t receive_tick(const TickShiftAcknowledgementBindings& bindings) {
    const auto time = current_engine_time(bindings.engine_client);
    const auto interval = field<float>(bindings.engine_globals, 0x20);
    // Native FDIV/FADD operate in ST0, then FSTP rounds once to float32
    // before CVTTSS2SI. Keep the original extended-return path on x86.
    volatile float rounded = static_cast<float>(time / static_cast<long double>(interval) + 0.5L);
    return _mm_cvttss_si32(_mm_set_ss(rounded));
}
std::int32_t process_ticks(const TickShiftAcknowledgementBindings& bindings) noexcept {
    auto duration = _mm_cvtepi32_ps(_mm_cvtsi32_si128(bindings.process_milliseconds));
    duration = _mm_div_ss(duration, _mm_set_ss(1000.0f));
    duration = _mm_min_ss(_mm_max_ss(duration, _mm_setzero_ps()), _mm_set_ss(1.0f));
    duration = _mm_div_ss(duration, _mm_set_ss(*bindings.interval));
    return _mm_cvttss_si32(_mm_add_ss(duration, _mm_set_ss(0.5f)));
}
}
void capture_tick_shift_acknowledgement(TickShiftAcknowledgementOwner& owner, void* player) {
    owner.captured_tick_base_118 = field<std::int32_t>(player, owner.bindings.tick_base_3d10b94c);
}
void finish_tick_shift_acknowledgement(TickShiftAcknowledgementOwner& owner, void* player) {
    auto& runtime = owner.runtime;
    auto& state = runtime.network;
    const auto& bindings = owner.bindings;
    if (!state.mode)
        return;
    const auto acknowledged = *bindings.acknowledged;
    if (acknowledged <= *bindings.old_acknowledged)
        return;
    if (field<std::uint8_t>(player, bindings.rejected_9d920e35))
        return;
    if (!acknowledgement_enabled(bindings.acknowledgement_convar.get()))
        return;
    if (field<std::int32_t>(bindings.engine_globals, 0x18) == 1)
        return;

    if (acknowledged >= runtime.transition_command_110) {
        runtime.transition_command_110 = (std::numeric_limits<std::int32_t>::max)();
        const auto threshold = subtract(receive_tick(bindings), 1);
        if (field<std::int32_t>(player, bindings.tick_base_3d10b94c) >= threshold)
            state.shot_tick = subtract(field<std::int32_t>(player, bindings.tick_base_3d10b94c), 1);
    }
    const auto command = engine::invoke<std::int32_t>(bindings.engine_client, 205);
    if (command < owner.phases.recharge_ready_command_11c || state.available < state.limit)
        return;
    const auto processing = process_ticks(bindings);
    const auto current = receive_tick(bindings);
    const auto difference = subtract(add(current, processing),
                                     field<std::int32_t>(player, bindings.tick_base_3d10b94c));
    if (std::uint32_t(difference) > 15u)
        return;
    if (runtime.generated_commands[std::uint32_t(command) % 150u] != command)
        return;

    if (state.history_count != 8)
        state.history_count = add(state.history_count, 1);
    state.history_first = (state.history_first - 1u) & 7u;
    state.history[state.history_first] = add(difference, 1);
}
} // namespace skeet::network
