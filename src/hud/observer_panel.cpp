#include "skeet/hud/observer_panel.hpp"
#include "skeet/esp/player_overlay.hpp"
#include <bit>

namespace skeet::hud {
void draw_observer_panel(ui::RenderQueue& queue, esp::PlayerFrameState& state,
                         const ObserverEngine& engine, const esp::RuntimePlayer* initial_view,
                         const esp::RuntimePlayer* observer, std::int32_t entity_index,
                         std::int32_t screen_width) {
    if (!initial_view || !observer || observer->observer_handle == -1)
        return;
    auto* target = engine.players.by_handle(engine.players.context, observer->observer_handle);
    if (!target)
        return;
    auto* view = esp::effective_view_player(engine.players);
    if (!view)
        return;
    const auto view_index = engine.players.index(engine.players.context, *view);
    const auto target_index = engine.players.index(engine.players.context, *target);
    if (target_index != view_index)
        return;
    ObserverPlayerInfo info;
    if (!engine.player_info(engine.context, entity_index, info) || info.hltv || info.fake_player)
        return;
    const auto name = esp::player_name({info.name.data(), info.name.size()});
    if (name.empty())
        return;
    const auto row = std::uint32_t(state.transient);
    state.transient = std::bit_cast<std::int32_t>(row + 1);
    const auto x = std::bit_cast<std::int32_t>(std::uint32_t(screen_width) - 10);
    const auto y = std::bit_cast<std::int32_t>(row * 20 + 5);
    queue.text(x, y, 0xC8FFFFFFu, 0x88, 0, name);
}
void draw_prepared_observer(ui::RenderQueue& queue, esp::PlayerFrameState& state,
                            const ObserverEngine& engine, const esp::RuntimePlayer* initial_view,
                            const esp::PreparedPlayer& prepared, std::int32_t screen_width) {
    if (prepared.stage == esp::PreparedPlayerStage::observer_panel)
        draw_observer_panel(queue, state, engine, initial_view, prepared.player,
                            prepared.entity_index, screen_width);
}
} // namespace skeet::hud
