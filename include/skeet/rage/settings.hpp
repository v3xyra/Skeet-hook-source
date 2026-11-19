#pragma once
#include "skeet/config.hpp"
#include "skeet/movement_config.hpp"
#include "experimental_switch.hpp"

namespace skeet::rage {
inline constexpr std::uint32_t experimental_rage_hash = 0xDCE86521u;
enum class ExperimentalRageOption { animation, resolver, records, extrapolated_pose, damage, hitchance, count };
inline constexpr std::array<std::uint32_t, std::size_t(ExperimentalRageOption::count)>
    experimental_rage_option_hashes{0x7C997C9Du, 0x8A4FC503u, 0x3EDE50EDu,
                                    0xC0303778u, 0x250AAD80u, 0xDEA7D2B2u};
inline constexpr std::array active_experimental_rage_options{
    ExperimentalRageOption::animation, ExperimentalRageOption::extrapolated_pose};
// Static descriptors retain their stored boolean values. The native startup
// routines write only the integer/key defaults identified below.
struct GlobalSettings {
    BooleanConfig enabled{{{nullptr, nullptr, 0x23060EC5u}, 1, ConfigType::boolean, 0},
                          0}; //0x43467214
    HotkeyConfig activation{{{nullptr, nullptr, 0x861E7F9Au}, 8, ConfigType::hotkey, 0},
                            {}}; //0x43467F24;6,hold
    IntegerConfig target_hitboxes{{{nullptr, nullptr, 0x5B900854u}, 4, ConfigType::integer, 0},
                                  0}; //0x434691BC;2
    HotkeyConfig multipoint_key{{{nullptr, nullptr, 0x29E2C8EDu}, 8, ConfigType::hotkey, 0},
                                {}}; //0x43469400;Escape,always
    BooleanConfig prefer_safe_point{{{nullptr, nullptr, 0xF5A00B3Bu}, 1, ConfigType::boolean, 0},
                                    0}; //0x43468F68
    HotkeyConfig force_safe_point{{{nullptr, nullptr, 0x333C1DBEu}, 8, ConfigType::hotkey, 0},
                                  {}}; //0x434678D0;Escape,hold
    IntegerConfig avoid_unsafe_hitboxes{
        {{nullptr, nullptr, 0xECBBD134u}, 4, ConfigType::integer, 0}, 0}; //0x43467020;0
    BooleanConfig prefer_body_aim{{{nullptr, nullptr, 0x2B54548Du}, 1, ConfigType::boolean, 0},
                                  0}; //0x43467D60
    IntegerConfig body_aim_disablers{{{nullptr, nullptr, 0x16C91A85u}, 4, ConfigType::integer, 0},
                                     0}; //0x4346A8C0;preserved
    HotkeyConfig force_body_aim{{{nullptr, nullptr, 0x02F59674u}, 8, ConfigType::hotkey, 0},
                                {}}; //0x4346A2F0;Escape,hold
    BooleanConfig quick_stop{{{nullptr, nullptr, 0xC06FD8CCu}, 1, ConfigType::boolean, 0},
                             0}; //0x434671D0
    HotkeyConfig quick_stop_key{{{nullptr, nullptr, 0x1AC79342u}, 8, ConfigType::hotkey, 0},
                                {}}; //0x43469308;Escape,always
    IntegerConfig quick_stop_options{{{nullptr, nullptr, 0x11195AD9u}, 4, ConfigType::integer, 0},
                                     0}; //0x43467E30;0
    IntegerConfig double_tap_hit_chance{
        {{nullptr, nullptr, 0x3F69C17Cu}, 4, ConfigType::integer, 0}, 0}; //0x4346A4A0;50
    IntegerConfig double_tap_quick_stop{
        {{nullptr, nullptr, 0x9FBDD0E6u}, 4, ConfigType::integer, 0},
        0}; // 0x4346A664; stored value survives
    BooleanConfig automatic_scope{{{nullptr, nullptr, 0x9E2B2E76u}, 1, ConfigType::boolean, 0},
                                  0}; //0x434693C4
    BooleanConfig anti_aim_correction{{{nullptr, nullptr, 0xA4D31376u}, 1, ConfigType::boolean, 0},
                                      0}; // 0x4346753C
    BooleanConfig automatic_fire{{{nullptr, nullptr, 0x59BD212Au}, 1, ConfigType::boolean, 0},
                                 0}; //0x43468400
    BooleanConfig automatic_penetration{
        {{nullptr, nullptr, 0x64715C40u}, 1, ConfigType::boolean, 0}, 0}; //0x4346A3D0
    BooleanConfig silent_aim{{{nullptr, nullptr, 0x86BFE317u}, 1, ConfigType::boolean, 0},
                             0}; //0x434684D0
    BooleanConfig remove_recoil{{{nullptr, nullptr, 0x63660060u}, 1, ConfigType::boolean, 0},
                                0}; //0x43467134
    HotkeyConfig duck_peek_assist{{{nullptr, nullptr, 0xD23ED2A7u}, 8, ConfigType::hotkey, 0},
                                  {}}; //0x43467C7C;Escape,hold
    BooleanConfig reduce_aim_step{{{nullptr, nullptr, 0x4034958Cu}, 1, ConfigType::boolean, 0},
                                  0}; // 0x434670B0
    IntegerConfig aim_step_limit{{{nullptr, nullptr, 0x20E889DEu}, 4, ConfigType::integer, 0},
                                 0}; // 0x4346708C; CRT default 15
    BooleanConfig pattern_recoil_correction{
        {{nullptr, nullptr, 0xA434F7C7u}, 1, ConfigType::boolean, 0}, 0}; //0x43467CA4
    BooleanConfig jitter_fix{{{nullptr, nullptr, 0x8C2E9D71u}, 1, ConfigType::boolean, 0}, 0};
    BooleanConfig experimental_rage{
        {{nullptr, nullptr, experimental_rage_hash}, 1, ConfigType::boolean, 0}, 0};
    std::array<BooleanConfig, experimental_rage_option_hashes.size()> experimental_options = [] {
        std::array<BooleanConfig, experimental_rage_option_hashes.size()> options{};
        for (const auto option : active_experimental_rage_options) {
            const auto i = std::size_t(option);
            options[i] = {{{nullptr, nullptr, experimental_rage_option_hashes[i]},
                           1, ConfigType::boolean, 0}, 1};
        }
        return options;
    }();

    ExperimentalSwitch experimental(ExperimentalRageOption option) const noexcept {
        return {&experimental_options[std::size_t(option)].value, &experimental_rage.value};
    }
};

// Configuration subset of owner 0x434803BC. Its visualization descriptors
// at +10C..+147 already belong to esp::AimbotVisualizationConfig.
struct AimbotSettings {
    IntegerConfig minimum_hit_chance; // +01C; 50
    IntegerConfig maximum_fov; // +030; 180
    IntegerConfig target_selection; // +044; 0
    IntegerConfig minimum_damage; // +058; 10
    BooleanConfig damage_override; // +06C
    HotkeyConfig damage_override_key; // +080; Escape, hold
    IntegerConfig override_damage; // +0A8; 10
    IntegerConfig multipoint_scale; // +0BC; 90
    IntegerConfig multipoint; // +0D0; 0
    IntegerConfig accuracy_boost; // +0E4; 1
    BooleanConfig delay_shot; // +0F8
    BooleanConfig force_body_aim_on_peek; // +15C
};

// One native owner (0x4347DEEC) supplies both RAGE tick-shift settings and the AA
// on-shot/fake-peek controls. These shared descriptors must be registered once.
struct TickShiftSettings {
    BooleanConfig on_shot_anti_aim; // +030
    HotkeyConfig on_shot_key; // +044; Escape, always
    IntegerConfig double_tap_fake_lag_limit; // +06C; 2
    BooleanConfig fake_peek; // +080
    HotkeyConfig fake_peek_key; // +094; X, hold
    BooleanConfig double_tap; // +0BC
    HotkeyConfig double_tap_key; // +0D0; Escape, hold
    IntegerConfig double_tap_mode; // +0F8; 0
};
struct ShotLogSettings {
    BooleanConfig spread_misses; // owner 0x4347F708, +038; true
    BooleanConfig detailed;
};
inline constexpr std::uint32_t detailed_shot_log_hash = 0x0E3BBCB6u;
struct PerformanceSettings {
    IntegerConfig mitigations; // owner 0x4347E6F2, +014; stored value survives
    BooleanConfig warning; // +028; shared MISC / Settings descriptor 0x740C32AE
};

void initialize_global_settings(ConfigRegistries&, GlobalSettings&) noexcept;
void initialize_aimbot_settings(ConfigRegistries&, AimbotSettings&) noexcept;
void initialize_tick_shift_settings(ConfigRegistries&, TickShiftSettings&) noexcept;
void initialize_shot_log_settings(ConfigRegistries&, ShotLogSettings&) noexcept;
void initialize_performance_settings(ConfigRegistries&, PerformanceSettings&) noexcept;

// The aggregate uses an explicit owner order; it does not reproduce process
// startup. Owner constructors preserve their local registration order/writes.
// GlobalSettings gathers independent startup routines in declaration order.
// Quick peek reuses the recovered movement-owner initializer.
struct Settings {
    GlobalSettings global;
    AimbotSettings aimbot;
    TickShiftSettings tick_shift;
    ShotLogSettings shot_log;
    PerformanceSettings performance;
    QuickPeekConfig quick_peek;
};
void initialize_settings(ConfigRegistries&, Settings&) noexcept;
} // namespace skeet::rage
