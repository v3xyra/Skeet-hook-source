#pragma once
#include "tick_shift_runtime.hpp"
#include "skeet/rage/settings.hpp"
#include <stdexcept>

namespace skeet::network {
enum class TickShiftControl {
    fake_peek,
    double_tap,
    on_shot
};
inline InputBitmap tick_shift_control_keys(const TickShiftRuntimeSettings& settings) noexcept {
    return settings.controls.keys;
}
// These reads stay at the native selection site after the engine gates.
// Enabling one control never captures the other controls or their hotkeys.
inline bool tick_shift_control_active(const TickShiftRuntimeSettings& settings, TickShiftControl control) {
    const auto& current = settings.controls.settings;
    switch (control) {
    case TickShiftControl::fake_peek:
        return current.fake_peek.value &&
               hotkey_active(current.fake_peek_key.value.binding, tick_shift_control_keys(settings));
    case TickShiftControl::double_tap:
        return current.double_tap.value &&
               hotkey_active(current.double_tap_key.value.binding, tick_shift_control_keys(settings));
    case TickShiftControl::on_shot:
        return current.on_shot_anti_aim.value &&
               hotkey_active(current.on_shot_key.value.binding, tick_shift_control_keys(settings));
    }
    throw std::invalid_argument("Unknown native tick-shift control");
}
inline std::int32_t tick_shift_fake_lag_setting(const TickShiftRuntimeSettings& settings) noexcept {
    return settings.controls.settings.double_tap_fake_lag_limit.value;
}
inline std::int32_t tick_shift_double_tap_setting(const TickShiftRuntimeSettings& settings) noexcept {
    return settings.controls.settings.double_tap_mode.value;
}
} // namespace skeet::network
