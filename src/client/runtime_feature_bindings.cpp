#include "skeet/client/runtime_feature_bindings.hpp"
#include "skeet/client/runtime_storage.hpp"
#include "skeet/client/native_startup.hpp"
#include "skeet/client/runtime_owner_initialization.hpp"
#include "skeet/client/runtime_owner_bindings.hpp"
#include "skeet/client/packet_runtime_owner.hpp"
#include "skeet/client/tick_shift_runtime_owner.hpp"
#include "skeet/client/misc_runtime_factories.hpp"
#include "skeet/client/skin_runtime_factories.hpp"
#include "skeet/client/movement_hud_factories.hpp"
#include "skeet/client/history_runtime_factories.hpp"
#include "skeet/client/world_runtime_factories.hpp"
#include "skeet/client/game_window_proxy_factory.hpp"
#include "skeet/client/source_terminal_cleanup.hpp"
#include "skeet/client/prediction_history_lifecycle.hpp"
#include "skeet/client/entity_command_lifecycle.hpp"
#include "skeet/client/target_mask_lifecycle.hpp"
#include "skeet/client/shared_esp_owner.hpp"
#include "skeet/client/buffered_shot_lifecycle.hpp"
#include "skeet/client/global_config_initializers.hpp"
#include "skeet/client/exit_registration.hpp"
#include "skeet/client/static_cleanup.hpp"
#include "skeet/anti_aim/runtime_owner.hpp"
#include "skeet/effects/runtime_owner.hpp"
#include "skeet/effects/free_look_owner.hpp"
#include "skeet/script/source_runtime_owner.hpp"
#include "skeet/script/ui_type_storage.hpp"
#include "skeet/rage/feedback_owner.hpp"
#include "skeet/rage/prediction_mask_owner.hpp"
#include "skeet/engine/map_game_system_owner.hpp"
#include "skeet/misc/viewmodel_lag_owner.hpp"
#include "skeet/hud/simple_owners.hpp"
#include <array>
#include <stdexcept>

namespace skeet::client {
namespace {
ClientRuntimeStorage& checked(ClientRuntimeStorage& storage, PacketRuntimeOwner& packet,
                              TickShiftRuntimeOwner& tick_shift, anti_aim::RuntimeOwner& aa,
                              effects::EffectsRuntimeOwner& effects,
                              script::SourceScriptRuntimeOwner& scripts) {
    if (&packet.storage() != &storage || &tick_shift.storage() != &storage ||
        &aa.storage() != &storage || &effects.storage() != &storage ||
        &scripts.storage() != &storage)
        throw std::invalid_argument(
            "Combat factory composition requires the same physical process owners");
    const auto& factory = scripts.factory_bindings();
    if (&factory.runtime != &scripts.runtime() || &factory.runtime.bridge != &scripts.bridge() ||
        &factory.modules != &scripts.module_bindings())
        throw std::invalid_argument(
            "Combat Lua factory requires its actual source runtime and bound modules");
    return storage;
}
void initialize_ping(void*) {
    hud::initialize_ping_hud_owner();
}
void initialize_crosshair(void*) {
    hud::initialize_crosshair_hud_owner();
}

void require_graph(NativeStartupSources& sources, NativeStartupSourceGraph& g) {
    auto& image = sources.data();
    if (&g.data != &image || g.final_initializer.native_address != 0x43484F06u ||
        !g.final_initializer.owner || !g.final_initializer.invoke)
        throw std::invalid_argument(
            "Late feature composition requires the canonical data and genuine final initializer context");
    // This checks the existing required operation; it never replaces its
    // owner/function or binds an empty stand-in for the remaining final work.
    sources.require_initializer(0, 0x43484F06u);
    if (&g.configuration.descriptors != &image.native_cell<ConfigLink>(0x434798BCu) ||
        &g.configuration.hotkeys != &image.native_cell<HotkeyLink>(0x434798C4u) ||
        &g.menu_key != &image.native_cell<RegisteredHotkey>(0x43478DB8u))
        throw std::invalid_argument(
            "Late feature factories require canonical configuration and menu-key cells");
    if (&g.prediction_history.configuration != &g.configuration ||
        &g.shared_esp.configuration != &g.configuration ||
        &g.shot_feedback.configuration != &g.configuration ||
        &g.runtime_owners.configuration != &g.configuration ||
        &g.misc_owners.configuration != &g.configuration ||
        &g.combat_owners.configuration != &g.configuration ||
        &g.skin_owners.configuration != &g.configuration ||
        &g.movement_hud.configuration != &g.configuration ||
        &g.world_owners.configuration != &g.configuration ||
        &g.runtime_owners.sequences.settings != &g.skins || &g.skin_owners.settings != &g.skins)
        throw std::invalid_argument(
            "Late feature groups require their one canonical configuration owner");
    const auto& registries = g.exits.cleanup.owner_registry_storage();
    if (&registries.image() != &image || g.entity_commands.data.get() != &image ||
        g.target_mask.data.get() != &image || g.history_owners.data.get() != &image ||
        g.combat_owners.data.get() != &image || &g.combat_owners.registries != &registries)
        throw std::invalid_argument(
            "Late feature groups require the same native data and lifecycle registry storage");
    struct Alias {
        const engine::OwnerRegistry* registry;
        std::uint32_t identity;
    };
    const std::array aliases{
        Alias{&g.prediction_history.registries.primary.registry, 0x43475B78u},
        Alias{&g.entity_commands.registries.lifecycle.registry, 0x43475B78u},
        Alias{&g.entity_commands.registries.map_entities.registry, 0x43475770u},
        Alias{&g.free_look.lifecycle.registry, 0x43475B78u},
        Alias{&g.free_look.updates.registry, 0x43475988u},
        Alias{&g.target_mask.registries.primary.registry, 0x43475B78u},
        Alias{&g.target_mask.registries.messages.registry, 0x43475B94u},
        Alias{&g.target_mask.registries.hud.registry, 0x43475A9Cu},
        Alias{&g.shared_esp.registries.primary.registry, 0x43475B78u},
        Alias{&g.shared_esp.registries.messages.registry, 0x43475B94u},
        Alias{&g.shot_feedback.lifecycle.registry, 0x43475B78u},
        Alias{&g.shot_feedback.packets.registry, 0x4347584Cu},
        Alias{&g.shot_feedback.frames.registry, 0x43475AD8u},
        Alias{&g.shot_feedback.positions.registry, 0x43475930u},
        Alias{&g.simple_hud.lifecycle.registry, 0x43475B78u},
        Alias{&g.simple_hud.hud.registry, 0x43475A9Cu},
        Alias{&g.prediction_masks.lifecycle.registry, 0x43475B78u},
        Alias{&g.prediction_masks.frames.registry, 0x43475AD8u}};
    for (const auto& alias : aliases)
        if (alias.registry != &registries.at(alias.identity))
            throw std::invalid_argument(
                "Late feature factories must retain their exact native owner registry cells");
    const auto* commands = &g.entity_commands.registries.commands;
    const auto* entities = &g.entity_commands.registries.entities;
    if (&commands->image() != &image || &entities->image() != &image ||
        &g.prediction_history.registries.commands != commands ||
        &g.target_mask.registries.commands != commands ||
        &g.shared_esp.registries.commands != commands || &g.shot_feedback.commands != commands ||
        &g.buffered_shots.lifecycle.commands != commands ||
        &g.viewmodel_lag.registries.commands != commands ||
        &g.runtime_owners.rage.command_owners != commands || &g.misc_owners.commands != commands ||
        &g.skin_owners.commands != commands || &g.combat_owners.packet.commands != commands ||
        &g.movement_hud.movement_registries.commands != commands ||
        &g.world_owners.command_owners != commands ||
        &g.runtime_owners.entity_updates != entities || &g.misc_owners.entities != entities ||
        &g.skin_owners.entity_updates != entities || &g.history_owners.entities != entities ||
        &g.target_mask.registries.entities != entities)
        throw std::invalid_argument(
            "Late feature factories require their one command and entity receiver registries");
    if (&g.entity_commands.registries.data_updates.image() != &image ||
        &g.entity_commands.registries.recharge.image() != &image ||
        &g.entity_commands.registries.prediction.image() != &image ||
        &g.prediction_history.registries.data_updates.image() != &image ||
        &g.prediction_history.registries.recharge.image() != &image ||
        &g.prediction_history.registries.prediction.image() != &image ||
        &g.prediction_history.registries.empty_ticks.image() != &image ||
        &g.shared_esp.registries.empty_ticks.image() != &image ||
        &g.shot_feedback.recharge.image() != &image ||
        &g.runtime_owners.legit.empty_tick_owners != &g.prediction_history.registries.empty_ticks)
        throw std::invalid_argument(
            "Late feature observers require their canonical prediction, recharge and empty-tick cells");
    if (&g.free_look.bindings.active != &image.native_cell<effects::FreeLookOwner*>(0x43475A64u) ||
        &g.shared_esp.bindings.active !=
            &image.native_cell<RejectedPlayerUpdateOwner*>(0x434758D0u) ||
        &g.shot_feedback.active != &image.native_cell<rage::FeedbackOwner*>(0x43475A7Cu) ||
        &g.prediction_masks.active != &image.native_cell<rage::PredictionMaskOwner*>(0x43475A58u) ||
        &g.runtime_owners.rage.commands.prediction_masks() != &g.prediction_masks.active ||
        &g.simple_hud.entity_list != &image.native_cell<void*>(0x4346A890u) ||
        &g.simple_hud.cvar != &image.native_cell<void*>(0x4346A87Cu) ||
        &g.simple_hud.suspended != &image.native_cell<std::uint8_t>(0x43468E58u) ||
        &g.simple_hud.ping_channel.bindings.history !=
            &image.native_cell<misc::PingSpikeNetworkHistory>(0x4347A500u))
        throw std::invalid_argument(
            "Late feature factories require canonical source singleton and HUD cells");
    if (&g.simple_hud.feature_indicators != &g.global_configuration.other_esp.feature_indicators ||
        &g.simple_hud.overlays != &g.global_configuration.other_esp.overlays ||
        &g.combat_owners.scripting.modules.ui_types != &g.ui_types.storage ||
        &g.source_terminal_cleanup.callbacks != &g.exits.callbacks ||
        &g.source_terminal_cleanup.bridge != &g.combat_owners.scripting.runtime.bridge ||
        &g.source_terminal_cleanup.bridge != &g.shot_feedback.state.scripts ||
        &g.shot_feedback.state != &g.history_owners.history.feedback ||
        &g.shot_feedback.runtime.players != &g.source_terminal_cleanup.players)
        throw std::invalid_argument(
            "Late feature factories require the actual source HUD, Lua, history and terminal producers");
    g.source_terminal_cleanup.require_producers(g.combat_owners.scripting.runtime,
                                                g.combat_owners.scripting.modules,
                                                g.history_owners.history.recording);
}
}

struct CombatRuntimeOwner::Data {
    ClientRuntimeStorage& storage;
    CombatRuntimeFactoryBindings factory;
    Data(ClientRuntimeStorage& s, PacketRuntimeOwner& packet, TickShiftRuntimeOwner& tick_shift,
         anti_aim::RuntimeOwner& aa, effects::EffectsRuntimeOwner& effects,
         script::SourceScriptRuntimeOwner& scripts)
        : storage(checked(s, packet, tick_shift, aa, effects, scripts)),
          factory{s.data_owner(),
                  s.owner_registries(),
                  s.configuration(),
                  packet.factory_bindings(),
                  tick_shift.factory_bindings(),
                  aa.factory_bindings(),
                  effects.factory_bindings(),
                  scripts.factory_bindings()} {}
};
CombatRuntimeOwner::CombatRuntimeOwner(ClientRuntimeStorage& s, PacketRuntimeOwner& packet,
                                       TickShiftRuntimeOwner& tick_shift, anti_aim::RuntimeOwner& aa,
                                       effects::EffectsRuntimeOwner& effects,
                                       script::SourceScriptRuntimeOwner& scripts)
    : data_(std::make_unique<Data>(s, packet, tick_shift, aa, effects, scripts)) {}
CombatRuntimeOwner::CombatRuntimeOwner(ClientRuntimeStorage& s, PacketRuntimeOwner& packet,
                                       TickShiftRuntimeOwner& tick_shift, anti_aim::RuntimeOwner& aa,
                                       effects::EffectsRuntimeOwner& effects,
                                       script::SourceScriptRuntimeOwner& scripts,
                                       script::SourceScriptModuleInputs inputs) {
    if (&scripts.storage() != &s)
        throw std::invalid_argument("Combat Lua modules must share this process storage");
    scripts.bind_modules(inputs);
    data_ = std::make_unique<Data>(s, packet, tick_shift, aa, effects, scripts);
}
CombatRuntimeOwner::~CombatRuntimeOwner() = default;
ClientRuntimeStorage& CombatRuntimeOwner::storage() const noexcept {
    return data_->storage;
}
CombatRuntimeFactoryBindings& CombatRuntimeOwner::factory_bindings() noexcept {
    return data_->factory;
}
void CombatRuntimeOwner::bind_initializers(NativeStartupSources& sources) {
    bind_combat_runtime_factories(sources, data_->factory);
}

void bind_runtime_feature_sources(NativeStartupSources& sources, NativeStartupSourceGraph& graph) {
    require_graph(sources, graph);
    bind_prediction_history_initializer(sources, graph.prediction_history);
    bind_native_entity_command_factory(sources, graph.entity_commands);
    effects::bind_free_look_initializer(sources, graph.free_look);
    bind_native_target_mask_factory(sources, graph.target_mask);
    bind_shared_esp_initializer(sources, graph.shared_esp);
    rage::bind_feedback_owner_factory(sources, graph.shot_feedback);
    bind_buffered_shot_factory(sources, graph.buffered_shots);
    misc::bind_viewmodel_lag_factory(sources, graph.viewmodel_lag);
    hud::bind_simple_hud_owners_context(graph.simple_hud);
    sources.bind_initializer({0x4347DD87u, &graph.simple_hud, initialize_ping});
    sources.bind_initializer({0x43486BF7u, &graph.simple_hud, initialize_crosshair});
    bind_runtime_owner_factories(sources, graph.runtime_owners);
    rage::bind_prediction_mask_factory(sources, graph.prediction_masks);
    engine::bind_map_game_system_factory(sources, graph.map_game_system);
    bind_misc_runtime_factories(sources, graph.misc_owners);
    bind_combat_runtime_factories(sources, graph.combat_owners);
    bind_skin_runtime_factories(sources, graph.skin_owners);
    bind_movement_hud_factories(sources, graph.movement_hud);
    bind_history_runtime_factories(sources, graph.history_owners);
    bind_world_runtime_factories(sources, graph.world_owners);
    bind_game_window_proxy_factory(sources, graph.game_window_proxy);
    preflight_runtime_owner_initialization(sources);
}
} //namespace skeet::client
