#include "skeet/hud/composite_handlers.hpp"
#include "skeet/engine/interface_call.hpp"
#include <cstring>

namespace skeet::hud {
void FreestandingHudBinding::draw(ui::RenderQueue& queue, void*, std::int32_t& cursor) const {
    draw_freestanding_indicator(queue, scripts, global, config, state, indicators, local, input,
                                cursor, layout);
}
void PingHudBinding::draw(ui::RenderQueue& queue, void*, std::int32_t& cursor) const {
    draw_ping_indicator(queue, scripts, indicators, local, ping, cursor, layout);
}
void RageHudBinding::draw(ui::RenderQueue& queue, void*, std::int32_t& cursor) const {
    draw_rage_indicators(queue, scripts, global, config, indicators, local, input, cursor, layout,
                         visualization, highlight, highlight_context);
}
void LiveRageHudBinding::draw(ui::RenderQueue& queue, void* player, std::int32_t& cursor) const {
    if (!highlight_context.life_state(player)) {
        //43468B94 is reloaded after each indicator/script callback. A callback
        //can change the next bit, hotkey, or damage-override setting this frame.
        if ((std::uint32_t(indicators.value) & 1u) &&
            hotkey_active(global.force_safe_point.value.binding, input))
            esp::draw_indicator(scripts, queue, cursor, "SAFE", 0xc8ffffffu, layout, -1, -1.0f);
        if ((std::uint32_t(indicators.value) & 2u) &&
            hotkey_active(global.force_body_aim.value.binding, input))
            esp::draw_indicator(scripts, queue, cursor, "BODY", 0xc8ffffffu, layout, -1, -1.0f);
        if ((std::uint32_t(indicators.value) & 256u) && config.damage_override.value &&
            hotkey_active(config.damage_override_key.value.binding, input))
            esp::draw_indicator(scripts, queue, cursor, "MD", 0xc8ffffffu, layout, -1, -1.0f);
    }
    esp::draw_live_hitbox_highlight(queue, visualization, highlight, player, highlight_context);
}
void TickShiftHudBinding::draw(ui::RenderQueue& queue, void* player, std::int32_t& cursor) const {
    draw_tick_shift_indicators(queue, scripts, state, shared_timer, indicators, player, interfaces,
                            frame_time, cursor, layout);
}
void MovementHudBinding::draw(ui::RenderQueue& queue, void*, std::int32_t& cursor) const {
    draw_movement_indicators(queue, scripts, indicators, duck_enabled, rage, command_flags,
                             quick_peek, state, local, input, cursor, layout, projection);
}
void CrosshairHudBinding::draw(ui::RenderQueue& queue, void* player, std::int32_t&) const {
    draw_crosshair(queue, config, interfaces, player, suspended, screen_size);
}
void PenetrationHudBinding::draw(ui::RenderQueue& queue, void* player, std::int32_t&) const {
    draw_penetration_reticle(queue, config, state, player, interfaces, screen_size);
}
void ScreenOverlayHudBinding::draw(ui::RenderQueue& queue, void* player, std::int32_t&) const {
    draw_screen_overlay(queue, config, state, player, life_state_offset, screen_size, projection,
                        view_origin, trigonometry);
}
void ScopeHudBinding::draw(ui::RenderQueue& queue, void* player, std::int32_t&) const {
    state.draw_scope(queue, screen_size, player);
}
void WorldHudBinding::draw(ui::RenderQueue&, void*, std::int32_t&) const {
    state.update();
}
void HitMarkerHudBinding::draw(ui::RenderQueue& queue, void* player, std::int32_t&) const {
    auto frame = view;
    frame.player = player;
    state.draw(queue, frame);
}
void PerformanceHudBinding::draw(ui::RenderQueue& queue, void* player, std::int32_t& cursor) const {
    std::uint8_t life;
    std::memcpy(&life, static_cast<const std::byte*>(player) + life_state_offset, sizeof(life));
    if (life || !config.warning.value || !low_fps || !*rage_enabled)
        return;
    if (!hotkey_active(rage.activation.value.binding, input))
        return;
    esp::draw_indicator(scripts, queue, cursor, "FPS", 0xc828ffffu, layout, -1, -1.0f);
}
void BlockbotHudBinding::draw(ui::RenderQueue& queue, void* player, std::int32_t&) const {
    std::uint8_t life;
    std::memcpy(&life, static_cast<const std::byte*>(player) + life_state_offset, sizeof(life));
    if (life || !state.activated || !(state.flags & 1u))
        return;
    const auto* origin = engine::invoke<const esp::Vector3*>(player, 10);
    const auto right = state.locked_right;
    const auto x = right.x * 64.0f, y = right.y * 64.0f, z = right.z * 64.0f;
    const esp::Vector3 first{origin->x + x, origin->y + y, origin->z + z};
    const esp::Vector3 second{origin->x - x, origin->y - y, origin->z - z};
    Point a, b;
    if (!esp::project_point(projection, first, a) || !esp::project_point(projection, second, b))
        return;
    queue.line({a.x, a.y, b.x, b.y, 0xc8000000u});
}
} // namespace skeet::hud
