#include "skeet/esp/player_engine.hpp"
#include "detail.hpp"

namespace skeet::esp {
using namespace detail;
namespace {
bool enemy_teams(std::int32_t a, std::int32_t b, EnginePlayers& source) {
    if (a == 2)
        return b == 3 || source.teammates_are_enemies();
    if (a == 3)
        return b == 2 || source.teammates_are_enemies();
    return false;
}
}
void update_player_geometry(PlayerGeometry& out, const RuntimePlayer& player, EnginePlayers& source,
                            Vector3 origin) {
    out.origin = origin;
    out.center = origin;
    const auto minimum = source.minimum(player);
    const auto maximum = source.maximum(player);
    out.center.z += (minimum.z + maximum.z) * 0.5f;
    out.eye_height = source.pose(player, 1) * 18.0f + 58.0f;
    out.pose_adjustment = source.pose(player, 12) * 6.0f;
    const auto second_maximum = source.maximum(player);
    out.lower = {out.center.x, out.center.y, out.center.z - second_maximum.z * 0.6f};
    out.upper = {out.center.x, out.center.y, (out.origin.z + out.eye_height) - out.pose_adjustment};
}
RuntimePlayer* effective_view_player(EnginePlayers& source) {
    auto* local = source.local();
    if (!local || source.life(*local) == 0 || source.movement(*local) != 10 ||
        (source.observer_mode(*local) != 4 && source.observer_mode(*local) != 5))
        return local;
    const auto handle = source.observer_target(*local);
    if (handle == -1)
        return local;
    auto* target = source.by_handle(handle);
    return target && source.is_player(*target) ? target : local;
}
bool live_enemy(const RuntimePlayer* first, const RuntimePlayer* second, EnginePlayers& source) {
    if (!first || !second || first == second)
        return false;
    if (source.game_type() == 6) {
        const auto squad = source.squad(*first);
        return squad == -1 || squad != source.squad(*second);
    }
    const auto a = source.team(*first), b = source.team(*second);
    return enemy_teams(a, b, source);
}
bool dormant_enemy(const RuntimePlayer* first, const RuntimePlayer* second, EnginePlayers& source) {
    if (!first || !second || first == second)
        return false;
    if (source.game_type() == 6) {
        const auto squad = source.squad(*first);
        return squad == -1 || squad != source.squad(*second);
    }
    auto* resource = source.resource();
    if (!resource)
        return false;
    const auto first_index = source.index(*first), second_index = source.index(*second);
    if (!source.connected(resource, first_index) || !source.connected(resource, second_index))
        return false;
    const auto b = source.resource_team(resource, second_index),
               a = source.resource_team(resource, first_index);
    return enemy_teams(a, b, source);
}
bool dormant_alive(const RuntimePlayer* player, EnginePlayers& source) {
    if (!player)
        return false;
    auto* resource = source.resource();
    if (!resource)
        return false;
    const auto index = source.index(*player);
    return source.connected(resource, index) && source.alive(resource, index);
}
PreparedPlayer prepare_live_player(ui::RenderQueue& queue, PlayerRuntimeState& state,
                                   const PlayerEspConfig& config, const WorldEspConfig& world,
                                   EnginePlayers& source, RuntimePlayer* initial_view,
                                   std::int32_t slot, bool activation, bool blocked) {
    PreparedPlayer output;
    auto reject_slot = [&] {
        state.geometry(std::uint32_t(slot)).alpha = 0.0f;
        return output;
    };
    if (!activation || blocked)
        return reject_slot();
    auto* player = source.by_index(slot);
    if (!player || player == initial_view || !source.is_player(*player) ||
        source.eligibility(*player))
        return reject_slot();
    const auto index = source.index(*player);
    auto& shared = *state.cache.get(std::uint32_t(index));
    auto& g = shared.geometry;
    auto reject = [&] {
        g.alpha = 0.0f;
        return output;
    };
    if (source.suppressed(*player))
        return reject();
    const auto dormant = source.dormant(*player);
    bool teammate, alive;
    if (dormant) {
        if (!source.resource())
            return reject();
        teammate = !dormant_enemy(initial_view, player, source);
        alive = dormant_alive(player, source);
    } else {
        teammate = !live_enemy(initial_view, player, source);
        alive = source.life(*player) == 0;
        if (alive) {
            const auto origin = source.origin(*player);
            update_player_geometry(g, *player, source, origin);
            const auto time = source.time();
            g.last_live_tick = truncate(float(static_cast<long double>(time) /
                                                  static_cast<long double>(source.tick_interval()) +
                                              0.5L));
        } else
            g.alpha = 0.0f;
        if (source.movement(*player) == 10 &&
            (source.observer_mode(*player) == 4 || source.observer_mode(*player) == 5)) {
            g.alpha = 0.0f;
            if (world.spectators.value)
                output = {PreparedPlayerStage::observer_panel,
                          player,
                          nullptr,
                          &g,
                          index,
                          false,
                          teammate,
                          &shared};
            return output;
        }
    }
    if (!alive || (!config.teammates.value && teammate))
        return reject();
    float alpha;
    if (dormant) {
        if (g.alpha == 0.0f)
            return output;
        if (!source.retain_dormant_alpha()) {
            g.alpha = sse_min(
                sse_max(g.alpha - source.frame_time() * (config.dormant.value ? 0.25f : 4.0f),
                        0.0f),
                1.0f);
            if (g.alpha == 0.0f)
                return output;
        }
        queue.set_desaturation(true);
        alpha = sse_min(0.8f, g.alpha);
    } else {
        alpha = sse_min(
            sse_max(g.alpha + source.frame_time() * std::bit_cast<float>(0x41555555u), 0.1f), 1.0f);
        g.alpha = alpha;
    }
    queue.set_alpha(alpha);
    auto* view = effective_view_player(source);
    if (!view || view == player || !source.is_player(*view)) {
        g.alpha = 0.0f;
        finish_player_overlay(queue);
        return output;
    }
    return {PreparedPlayerStage::overlay, player, view, &g, index, dormant, teammate, &shared};
}
void draw_live_observer(ui::RenderQueue& queue, PlayerFrameState& state, EnginePlayers& source,
                        RuntimePlayer* initial_view, const PreparedPlayer& prepared) {
    if (prepared.stage != PreparedPlayerStage::observer_panel || !initial_view || !prepared.player)
        return;
    const auto handle = source.observer_target(*prepared.player);
    if (handle == -1)
        return;
    auto* target = source.by_handle(handle);
    if (!target)
        return;
    auto* view = effective_view_player(source);
    if (!view)
        return;
    const auto view_index = source.index(*view), target_index = source.index(*target);
    if (view_index != target_index)
        return;
    hud::ObserverPlayerInfo info;
    if (!source.player_info(prepared.entity_index, info) || info.hltv || info.fake_player)
        return;
    const auto name = player_name({info.name.data(), info.name.size()});
    if (name.empty())
        return;
    const auto row = std::uint32_t(state.transient);
    state.transient = std::bit_cast<std::int32_t>(row + 1);
    const auto width = queue.screen().x;
    queue.text(std::bit_cast<std::int32_t>(std::uint32_t(width) - 10),
               std::bit_cast<std::int32_t>(row * 20 + 5), 0xc8ffffffu, 0x88, 0, name);
}
} //namespace skeet::esp
