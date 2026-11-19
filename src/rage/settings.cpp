#include "skeet/rage/settings.hpp"

namespace skeet::rage {
void initialize_global_settings(ConfigRegistries& r, GlobalSettings& c) noexcept {
    auto& d = r.descriptors;
    initialize_boolean_config(d, c.enabled, 0x23060EC5);
    initialize_hotkey_config(r, c.activation, 0x861E7F9A, 6, HotkeyMode::hold);
    initialize_integer_config(d, c.target_hitboxes, 0x5B900854, 2);
    initialize_hotkey_config(r, c.multipoint_key, 0x29E2C8ED, 0x1B, HotkeyMode::always);
    initialize_boolean_config(d, c.prefer_safe_point, 0xF5A00B3B);
    initialize_hotkey_config(r, c.force_safe_point, 0x333C1DBE, 0x1B, HotkeyMode::hold);
    initialize_integer_config(d, c.avoid_unsafe_hitboxes, 0xECBBD134, 0);
    initialize_boolean_config(d, c.prefer_body_aim, 0x2B54548D);
    initialize_integer_config(d, c.body_aim_disablers, 0x16C91A85, c.body_aim_disablers.value);
    initialize_hotkey_config(r, c.force_body_aim, 0x02F59674, 0x1B, HotkeyMode::hold);
    initialize_boolean_config(d, c.quick_stop, 0xC06FD8CC);
    initialize_hotkey_config(r, c.quick_stop_key, 0x1AC79342, 0x1B, HotkeyMode::always);
    initialize_integer_config(d, c.quick_stop_options, 0x11195AD9, 0);
    initialize_integer_config(d, c.double_tap_hit_chance, 0x3F69C17C, 50);
    initialize_integer_config(d, c.double_tap_quick_stop, 0x9FBDD0E6,
                              c.double_tap_quick_stop.value);
    initialize_boolean_config(d, c.automatic_scope, 0x9E2B2E76);
    initialize_boolean_config(d, c.anti_aim_correction, 0xA4D31376);
    initialize_boolean_config(d, c.automatic_fire, 0x59BD212A);
    initialize_boolean_config(d, c.automatic_penetration, 0x64715C40);
    initialize_boolean_config(d, c.silent_aim, 0x86BFE317);
    initialize_boolean_config(d, c.remove_recoil, 0x63660060);
    initialize_hotkey_config(r, c.duck_peek_assist, 0xD23ED2A7, 0x1B, HotkeyMode::hold);
    initialize_boolean_config(d, c.reduce_aim_step, 0x4034958C);
    initialize_integer_config(d, c.aim_step_limit, 0x20E889DE, 15);
    initialize_boolean_config(d, c.pattern_recoil_correction, 0xA434F7C7);
    initialize_boolean_config(d, c.experimental_rage, experimental_rage_hash);
    for (const auto option : active_experimental_rage_options) {
        const auto i = std::size_t(option);
        initialize_boolean_config(d, c.experimental_options[i], experimental_rage_option_hashes[i]);
    }
}
void initialize_aimbot_settings(ConfigRegistries& r, AimbotSettings& c) noexcept {
    auto& d = r.descriptors;
    initialize_integer_config(d, c.minimum_hit_chance, 0x3F5E5553, 50);
    initialize_integer_config(d, c.maximum_fov, 0xD6D22A5A, 180);
    initialize_integer_config(d, c.target_selection, 0x65E86322, 0);
    initialize_integer_config(d, c.minimum_damage, 0x18C0E6BE, 10);
    initialize_boolean_config(d, c.damage_override, 0xB80303B4);
    initialize_hotkey_config(r, c.damage_override_key, 0x7EB3A142, 0x1B, HotkeyMode::hold);
    initialize_integer_config(d, c.override_damage, 0x7DC413FE, 10);
    initialize_integer_config(d, c.multipoint_scale, 0xA25029BA, 90);
    initialize_integer_config(d, c.multipoint, 0xBFABB4A0, 0);
    initialize_integer_config(d, c.accuracy_boost, 0x4346EDB5, 1);
    initialize_boolean_config(d, c.delay_shot, 0xA09C0D52);
    initialize_boolean_config(d, c.force_body_aim_on_peek, 0x9BBD48EA);
}
void initialize_tick_shift_settings(ConfigRegistries& r, TickShiftSettings& c) noexcept {
    auto& d = r.descriptors;
    initialize_boolean_config(d, c.on_shot_anti_aim, 0xB2467427);
    initialize_hotkey_config(r, c.on_shot_key, 0x8F4F3C91, 0x1B, HotkeyMode::always);
    initialize_integer_config(d, c.double_tap_fake_lag_limit, 0x2B56E048, 2);
    initialize_boolean_config(d, c.fake_peek, 0x8494FC0D);
    initialize_hotkey_config(r, c.fake_peek_key, 0xAE772257, 0x58, HotkeyMode::hold);
    initialize_boolean_config(d, c.double_tap, 0x0889939F);
    initialize_hotkey_config(r, c.double_tap_key, 0x67C9986D, 0x1B, HotkeyMode::hold);
    initialize_integer_config(d, c.double_tap_mode, 0x31E2D7DD, 0);
}
void initialize_shot_log_settings(ConfigRegistries& r, ShotLogSettings& c) noexcept {
    initialize_boolean_config(r.descriptors, c.spread_misses, 0xE82BE8DD);
    c.spread_misses.value = 1; // 0x4347F7BA, after registration
}
void initialize_performance_settings(ConfigRegistries& r, PerformanceSettings& c) noexcept {
    initialize_integer_config(r.descriptors, c.mitigations, 0x2B385DC7, c.mitigations.value);
    initialize_boolean_config(r.descriptors, c.warning, 0x740C32AE);
}
void initialize_settings(ConfigRegistries& r, Settings& c) noexcept {
    initialize_global_settings(r, c.global);
    initialize_aimbot_settings(r, c.aimbot);
    initialize_tick_shift_settings(r, c.tick_shift);
    initialize_shot_log_settings(r, c.shot_log);
    initialize_performance_settings(r, c.performance);
    initialize_quick_peek_config(r, c.quick_peek);
}
} // namespace skeet::rage
