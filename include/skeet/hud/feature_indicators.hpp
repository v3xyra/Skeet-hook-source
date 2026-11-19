#pragma once
#include "skeet/anti_aim/config.hpp"
#include "skeet/anti_aim/state.hpp"
#include "skeet/esp/player_runtime.hpp"
#include "skeet/misc/ping_spike.hpp"
#include "skeet/rage/settings.hpp"
#include "skeet/esp/hitbox_highlight.hpp"
#include "skeet/network/packet_control.hpp"

namespace skeet::hud {
// Complete valid-player render owner 433E8FD3. This consumes the native
// feature-indicator descriptor 59A226C2 and the anti-aim owner descriptors.
void draw_freestanding_indicator(ui::RenderQueue& queue, script::Bridge& scripts,
                                 const anti_aim::GlobalConfig& global,
                                 const anti_aim::Config& config, anti_aim::State& state,
                                 const IntegerConfig& feature_indicators,
                                 const esp::RuntimePlayer& local_player, InputBitmap input,
                                 std::int32_t& cursor, const esp::IndicatorLayout& layout);
// Complete valid-player 433DF3FC. Calls the recovered PingSpikeRuntime::active
// owner and engine/channel virtuals, then the native HSV color/indicator chain.
void draw_ping_indicator(ui::RenderQueue& queue, script::Bridge& scripts,
                         const IntegerConfig& feature_indicators,
                         const esp::RuntimePlayer& local_player, const misc::PingSpikeRuntime& ping,
                         std::int32_t& cursor, const esp::IndicatorLayout& layout);
// Admitted433DF454..433DF54E: actual channel/latencies and native HSV draw.
// Both the semantic entry and the exact-layout HUD owner use this same body.
void draw_ping_latency_indicator(ui::RenderQueue&, script::Bridge&, const misc::PingSpikeRuntime&,
                                 std::int32_t& cursor, const esp::IndicatorLayout&);
// Complete valid-player 4337F589, including the unconditional concrete
// hitbox-highlight successor 433D12F7 after SAFE, BODY, MD in native order.
void draw_rage_indicators(ui::RenderQueue& queue, script::Bridge& scripts,
                          const rage::GlobalSettings& global, const rage::AimbotSettings& config,
                          const IntegerConfig& feature_indicators,
                          const esp::RuntimePlayer& local_player, InputBitmap input,
                          std::int32_t& cursor, const esp::IndicatorLayout& layout,
                          const esp::AimbotVisualizationConfig& visualization,
                          esp::HitboxHighlightState& highlight_state,
                          const esp::HitboxHighlightContext& highlight_context);
struct TickShiftIndicatorBindings {
    void* entity_list = nullptr;
    std::uint32_t life_state, active_weapon, weapon_definition;
};
// Complete valid-player 433E1B2F. Both timers decay before the mode gate.
// shared_timer is native global43475890; state is the network interface's
// shared tick-shift state, whose timer+124 aliases render-interface+134.
void draw_tick_shift_indicators(ui::RenderQueue& queue, script::Bridge& scripts,
                             network::TickShiftState& state, float& shared_timer,
                             const IntegerConfig& feature_indicators, void* local_player,
                             const TickShiftIndicatorBindings& bindings, float frame_time,
                             std::int32_t& cursor, const esp::IndicatorLayout& layout);
} // namespace skeet::hud
