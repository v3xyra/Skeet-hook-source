#include "skeet/hud/feature_indicators.hpp"
#include "skeet/color_picker.hpp"
#include "skeet/engine/interface_call.hpp"
#include <immintrin.h>
#include <cstring>

namespace skeet::hud {
void draw_freestanding_indicator(ui::RenderQueue& queue, script::Bridge& scripts,
                                 const anti_aim::GlobalConfig& global,
                                 const anti_aim::Config& config, anti_aim::State& state,
                                 const IntegerConfig& feature_indicators,
                                 const esp::RuntimePlayer& local, InputBitmap input,
                                 std::int32_t& cursor, const esp::IndicatorLayout& layout) {
    if (!global.enabled.value)
        return;
    if (local.life_state != 0) {
        state.value_1b4 = -1;
        state.value_1b0 = 0;
        return;
    }
    if (!(std::uint32_t(feature_indicators.value) & 32) || !config.freestanding.value)
        return;
    if (!hotkey_active(config.freestanding_hotkey.value.binding, input))
        return;
    esp::draw_indicator(scripts, queue, cursor, "FS", 0xC8FFFFFFu, layout, -1, -1.0f);
}
void draw_ping_indicator(ui::RenderQueue& queue, script::Bridge& scripts,
                         const IntegerConfig& feature_indicators, const esp::RuntimePlayer& local,
                         const misc::PingSpikeRuntime& ping, std::int32_t& cursor,
                         const esp::IndicatorLayout& layout) {
    if (!(std::uint32_t(feature_indicators.value) & 4) || local.life_state || !ping.active())
        return;
    draw_ping_latency_indicator(queue, scripts, ping, cursor, layout);
}
void draw_ping_latency_indicator(ui::RenderQueue& queue, script::Bridge& scripts,
                                 const misc::PingSpikeRuntime& ping, std::int32_t& cursor,
                                 const esp::IndicatorLayout& layout) {
    auto* channel = engine::invoke<void*>(ping.engine, 78);
    if (!channel)
        return;
    const auto latency = reinterpret_cast<engine::MemberFunction<float, std::int32_t>>(
        (*static_cast<void***>(channel))[9]);
    const auto first = latency(channel, 0);
    const auto second = latency(channel, 1);
    const auto sum = float(static_cast<long double>(second) + static_cast<long double>(first));
    const auto requested =
        _mm_mul_ss(_mm_cvtepi32_ps(_mm_cvtsi32_si128(*ping.milliseconds)), _mm_set_ss(-0.001f));
    auto denominator =
        _mm_sub_sd(_mm_set_sd(*ping.latency_4347a508), _mm_cvtss_sd(_mm_setzero_pd(), requested));
    denominator = _mm_add_sd(denominator, _mm_set_sd(*ping.latency_4347b120));
    const auto divisor =
        _mm_min_ss(_mm_max_ss(_mm_cvtsd_ss(_mm_setzero_ps(), denominator), _mm_set_ss(0.001f)),
                   _mm_set_ss(0.2f));
    const auto ratio = _mm_cvtss_f32(_mm_min_ss(
        _mm_max_ss(_mm_div_ss(_mm_set_ss(sum), divisor), _mm_setzero_ps()), _mm_set_ss(1)));
    std::uint32_t color;
    if (ratio <= 0)
        color = 0xC8FFFFFFu;
    else if (ratio >= 1)
        color = 0xFF15C28Fu;
    else {
        const auto from = ui::rgb_to_hsv(0xC8FFFFFFu), to = ui::rgb_to_hsv(0xFF15C28Fu);
        color = ui::hsv_to_rgb({(to.hue - from.hue) * ratio + from.hue,
                                (to.saturation - from.saturation) * ratio + from.saturation,
                                (to.value - from.value) * ratio + from.value});
    }
    esp::draw_indicator(scripts, queue, cursor, "PING", color, layout, -1, -1.0f);
}
void draw_rage_indicators(ui::RenderQueue& queue, script::Bridge& scripts,
                          const rage::GlobalSettings& global, const rage::AimbotSettings& config,
                          const IntegerConfig& feature_indicators, const esp::RuntimePlayer& local,
                          InputBitmap input, std::int32_t& cursor,
                          const esp::IndicatorLayout& layout,
                          const esp::AimbotVisualizationConfig& visualization,
                          esp::HitboxHighlightState& highlight_state,
                          const esp::HitboxHighlightContext& highlight_context) {
    if (!local.life_state) {
        const auto mask = std::uint32_t(feature_indicators.value);
        if ((mask & 1) && hotkey_active(global.force_safe_point.value.binding, input))
            esp::draw_indicator(scripts, queue, cursor, "SAFE", 0xC8FFFFFFu, layout, -1, -1.0f);
        if ((mask & 2) && hotkey_active(global.force_body_aim.value.binding, input))
            esp::draw_indicator(scripts, queue, cursor, "BODY", 0xC8FFFFFFu, layout, -1, -1.0f);
        if ((mask & 256) && config.damage_override.value &&
            hotkey_active(config.damage_override_key.value.binding, input))
            esp::draw_indicator(scripts, queue, cursor, "MD", 0xC8FFFFFFu, layout, -1, -1.0f);
    }
    esp::draw_hitbox_highlight(queue, visualization, highlight_state, local, highlight_context);
}
#pragma STDC FENV_ACCESS ON
void draw_tick_shift_indicators(ui::RenderQueue& queue, script::Bridge& scripts,
                             network::TickShiftState& state, float& shared_timer,
                             const IntegerConfig& feature_indicators, void* local,
                             const TickShiftIndicatorBindings& bindings, float frame_time,
                             std::int32_t& cursor, const esp::IndicatorLayout& layout) {
    state.timer_124 = maximum_number(0.0f, state.timer_124 - frame_time);
    shared_timer = maximum_number(0.0f, shared_timer - frame_time);
    if (!state.mode)
        return;
    std::uint8_t life;
    std::memcpy(&life, static_cast<std::byte*>(local) + bindings.life_state, sizeof(life));
    if (life) {
        shared_timer = 0.0f;
        return;
    }
    const auto mask = std::uint32_t(feature_indicators.value);
    if (state.mode == 1) {
        std::int32_t handle;
        std::memcpy(&handle, static_cast<std::byte*>(local) + bindings.active_weapon,
                    sizeof(handle));
        if (handle == -1)
            return;
        auto* weapon = engine::invoke<void*>(bindings.entity_list, 4, handle);
        if (!weapon || !(mask & 8))
            return;
        bool unavailable = state.available < state.limit;
        if (!unavailable) {
            std::uint16_t definition;
            std::memcpy(&definition, static_cast<std::byte*>(weapon) + bindings.weapon_definition,
                        sizeof(definition));
            unavailable = definition == 31;
        }
        esp::draw_indicator(scripts, queue, cursor, "DT", unavailable ? 0xFF3200FFu : 0xC8FFFFFFu,
                            layout, -1, -1.0f);
    } else if (state.mode == 2 && (mask & 64))
        esp::draw_indicator(scripts, queue, cursor, "OSAA", 0xC8FFFFFFu, layout, -1, -1.0f);
}
#pragma STDC FENV_ACCESS OFF
} // namespace skeet::hud
