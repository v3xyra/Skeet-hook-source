#include "skeet/client/source_feature_runtime.hpp"
#include "skeet/network/user_message_runtime.hpp"
#include "skeet/client/source_feature_runtime_imports.hpp"
#include "skeet/client/runtime_storage.hpp"
#include "skeet/client/runtime_composition.hpp"
#include "skeet/client/player_runtime_owner.hpp"
#include "skeet/client/history_runtime_owner.hpp"
#include "skeet/client/visual_runtime_owner.hpp"
#include "skeet/client/command_capture_runtime_owner.hpp"
#include "skeet/client/command_runtime_owner.hpp"
#include "skeet/client/local_animation_runtime_owner.hpp"
#include "skeet/client/movement_hud_runtime_owner.hpp"
#include "skeet/client/target_refresh_runtime_owner.hpp"
#include "skeet/client/shared_esp_runtime_owner.hpp"
#include "skeet/client/misc_runtime_owner.hpp"
#include "skeet/client/packet_runtime_owner.hpp"
#include "skeet/client/tick_shift_runtime_owner.hpp"
#include "skeet/client/entity_command_runtime_owner.hpp"
#include "skeet/client/prediction_history_runtime_owner.hpp"
#include "skeet/client/buffered_shot_runtime_owner.hpp"
#include "skeet/client/target_mask_runtime_owner.hpp"
#include "skeet/client/feature_runtime_owner.hpp"
#include "skeet/client/runtime_feature_bindings.hpp"
#include "skeet/client/runtime_owner_initialization.hpp"
#include "skeet/client/world_runtime_owner.hpp"
#include "skeet/client/player_receive_owner.hpp"
#include "skeet/client/shutdown_runtime_owner.hpp"
#include "skeet/config_storage/owner.hpp"
#include "skeet/engine/scene_runtime_owner.hpp"
#include "skeet/engine/hook_runtime_owner.hpp"
#include "skeet/engine/client_class_factory_hooks.hpp"
#include "skeet/engine/build_hook_initialization.hpp"
#include "skeet/engine/player_hook_family.hpp"
#include "skeet/engine/map_game_system_owner.hpp"
#include "skeet/animation/runtime_owner.hpp"
#include "skeet/animation/fix_runtime_owner.hpp"
#include "skeet/animation/fix_animation_backup_cleanup.hpp"
#include "skeet/effects/runtime_owner.hpp"
#include "skeet/effects/camera_runtime_owner.hpp"
#include "skeet/effects/player_opacity_owner.hpp"
#include "skeet/effects/free_look_runtime_owner.hpp"
#include "skeet/effects/visual_receive_owner.hpp"
#include "skeet/effects/player_auxiliary_hooks.hpp"
#include "skeet/effects/harmless_molotov.hpp"
#include "skeet/esp/runtime_owner.hpp"
#include "skeet/rage/runtime_owner.hpp"
#include "skeet/rage/feedback_runtime_owner.hpp"
#include "skeet/rage/prediction_mask_runtime_owner.hpp"
#include "skeet/legit/runtime_owner.hpp"
#include "skeet/anti_aim/runtime_owner.hpp"
#include "skeet/script/api_runtime_owner.hpp"
#include "skeet/skins/runtime_owner.hpp"
#include "skeet/misc/settings_runtime_owner.hpp"
#include "skeet/misc/viewmodel_runtime_owner.hpp"
#include "skeet/hud/simple_runtime_owner.hpp"
#include "skeet/render/renderer_assets.hpp"
#include "skeet/render/startup_renderer.hpp"
#include "skeet/menu/startup_window.hpp"
#include "skeet/players/menu_bindings.hpp"
#include "skeet/network/packet_dispatch.hpp"
#include "skeet/network/client_state_runtime_owner.hpp"
#include <stdexcept>

namespace skeet::client {
namespace {
SourceFeatureInputs checked(SourceFeatureInputs inputs) {
    auto& s = inputs.storage;
    if (&inputs.engine.variables.data() != &s.data() || &inputs.crt.storage() != &s ||
        &inputs.configuration.storage() != &s || &inputs.static_crt.storage() != &s ||
        &inputs.window.storage() != &s || &inputs.players.storage() != &s ||
        &inputs.scripts.storage() != &s || &inputs.assets.storage() != &s ||
        &inputs.shutdown.storage() != &s || &inputs.materials.data != &s.data() ||
        &inputs.skin_engine.data != &s.data() ||
        &inputs.presets.native_storage() != &s.config_storage() ||
        &inputs.main_window.menu_data() != &s.menu_startup_data() ||
        &inputs.animation_cleanup.callbacks != &s.exit_callbacks() ||
        &inputs.scripts.unload_state() != &inputs.window.unload_state() ||
        &inputs.assets.startup_renderer() != &inputs.renderer)
        throw std::invalid_argument(
            "Feature assembly requires the one canonical process and engine lifetime");
    if (inputs.final_initializer.native_address != 0x43484F06u || !inputs.final_initializer.owner ||
        !inputs.final_initializer.invoke)
        throw std::invalid_argument(
            "Feature assembly requires its actual complete final startup operation");
    if (!s.exit_callbacks().count)
        throw std::logic_error("Feature assembly must follow the real process CRT initialization");
    return inputs;
}
engine::OwnerRegistryBinding registry(ClientRuntimeStorage& s, std::uint32_t identity) {
    return {s.owner_registries().at(identity), s.memory()};
}
SourceFeatureRuntimeImports native_imports(const SourceFeatureInputs& in) {
    return {native_history_runtime_imports(), native_misc_runtime_imports(in.engine),
            native_packet_runtime_imports(in.storage, in.engine),
            anti_aim::native_runtime_imports(in.storage, in.engine),
            script::native_script_api_runtime_imports(in.engine)};
}
}

struct SourceFeatureRuntime::Data {
    SourceFeatureInputs inputs;
    engine::SceneQueryRuntimeOwner scene;
    animation::RuntimeOwner bones;
    HistoryRuntimeOwner history;
    VisualRuntimeOwner visual;
    effects::EffectsRuntimeOwner effects;
    effects::CameraRuntimeOwner camera;
    effects::PlayerOpacityOwner opacity;
    engine::HookRuntimeOwner hooks;
    animation::FixRuntimeOwner fix;
    CommandCaptureRuntimeOwner capture;
    LocalAnimationRuntimeOwner local;
    MovementHudRuntimeOwner movement;
    TargetRefreshRuntimeOwner target_refresh;
    SharedEspRuntimeOwner shared_esp;
    CommandRuntimeOwner commands;
    esp::EspRuntimeOwner esp;
    rage::RuntimeOwner rage;
    legit::RuntimeOwner legit;
    MiscRuntimeOwner miscellaneous;
    script::ScriptApiRuntimeOwner script_api;
    skins::SkinsRuntimeOwner skins;
    PacketRuntimeOwner packet;
    anti_aim::RuntimeOwner anti_aim;
    TickShiftRuntimeOwner tick_shift;
    misc::SettingsRuntimeOwner settings;
    PredictionHistoryRuntimeOwner prediction_history;
    EntityCommandRuntimeOwner entity_commands;
    engine::ClientClassFactoryRuntime class_factories;
    misc::ViewmodelRuntimeOwner viewmodel;
    effects::FreeLookRuntimeOwner free_look;
    TargetMaskRuntimeOwner target_mask;
    rage::FeedbackRuntimeOwner feedback;
    BufferedShotRuntimeOwner buffered_shots;
    rage::PredictionMaskRuntimeOwner prediction_masks;
    PlayerReceiveOwner player_receives;
    effects::VisualReceiveOwner visual_receives;
    players::MenuBindings player_menu;
    FeatureRuntimeOwner features;
    engine::MapGameSystemRuntime map_system;
    network::PacketDispatchRuntime packets;
    network::UserMessageRuntime user_messages;
    std::unique_ptr<engine::PlayerHookFamily> player_hooks;
    std::unique_ptr<CombatRuntimeOwner> combat;
    std::unique_ptr<WorldRuntimeOwner> world;
    std::unique_ptr<hud::SimpleHudRuntimeOwner> simple_hud;
    std::unique_ptr<NativeStartupSourceGraph> graph;
    bool factories_started = false, hook_contexts_bound = false;

    Data(SourceFeatureInputs in, const SourceFeatureRuntimeImports& imports)
        : inputs(checked(in)), scene(in.storage, in.engine, in.players),
          bones(in.storage, in.engine, in.players.cache()),
          history(in.storage, in.engine, in.configuration, in.players, scene, bones, in.scripts,
                  imports.history),
          visual(in.storage, in.engine, in.configuration, in.renderer, in.assets),
          effects(in.storage, in.engine, in.configuration, visual, in.materials),
          camera(in.storage, in.engine, in.configuration, in.players),
          opacity(in.storage, in.engine, in.configuration, in.players),
          hooks(in.crt, in.scripts, in.renderer),
          fix(bones, in.configuration, hooks, opacity.bindings(), in.animation_cleanup),
          capture(in.storage, in.engine, in.configuration, in.players, bones,
                  history.entity_runtime()),
          local(in.storage, in.engine, in.configuration, history, capture, effects),
          movement(in.storage, in.engine, in.configuration, scene, visual, in.scripts, capture,
                   history),
          target_refresh(in.storage, in.engine, in.players, history, scene, bones, movement),
          shared_esp(in.storage, in.engine, in.configuration, in.players, history, in.scripts),
          commands(in.storage, in.engine, in.configuration, history, capture, bones),
          esp(in.storage, in.engine, in.configuration, in.players, scene, visual, in.crt,
              in.scripts, in.assets, {in.storage.data()}),
          rage(in.storage, in.engine, in.configuration, history, scene, bones, capture, movement,
               target_refresh, in.crt, esp, effects, visual, in.scripts,
               rage::RuntimeImports{imports.history.thread_pool}),
          legit(in.storage, in.engine, in.configuration, history, scene, bones, capture, in.crt,
                esp),
          miscellaneous(in.storage, in.engine, in.configuration, in.crt.descriptor_storage(),
                        {in.scripts.bridge(),
                         scene,
                         bones,
                         rage,
                         movement.live_movement(),
                         capture.budget(),
                         visual.projection(),
                         visual.renderer(),
                         visual.explosive_texture(),
                         visual.fire_texture(),
                         visual.ribbons(),
                         visual.indicators(),
                         {in.storage.data().native_cell<MovementCommandOwner*>(0x434759B0u),
                          movement.release_movement()},
                         MovementFeedbackBinding(
                             in.storage.data().native_cell<MovementCommandOwner*>(0x434759B0u))},
                        imports.miscellaneous),
          script_api(in.storage, in.engine, in.configuration, in.players, history, scene, bones,
                     visual, effects, hooks, in.assets, miscellaneous, in.scripts,
                     imports.script_api),
          skins(in.storage, in.configuration, in.skin_engine),
          packet(in.storage, in.engine, in.configuration, history, movement, target_refresh,
                 imports.packet),
          anti_aim(in.storage, in.engine, in.configuration, scene, bones, movement, capture,
                   history, target_refresh, visual, in.scripts, imports.anti_aim),
          tick_shift(in.storage, in.engine, in.configuration, history, movement, commands, local,
                  scene, visual, in.scripts),
          settings(in.storage, in.configuration, in.main_window, in.presets, in.shutdown.request()),
          prediction_history(in.storage, in.engine, in.configuration, local, history, movement,
                             bones, capture),
          entity_commands(in.storage, in.engine, in.configuration, history, local, capture),
          class_factories(in.storage, in.engine, in.players, entity_commands, hooks),
          viewmodel(in.storage, in.engine, in.configuration, skins, effects, hooks),
          free_look(in.storage, in.engine, in.configuration, in.crt),
          target_mask(in.storage, in.engine, in.configuration),
          feedback(in.storage, in.engine, in.configuration, history, scene, bones, in.crt, esp,
                   script_api),
          buffered_shots(in.storage, in.engine, in.configuration, history, capture, bones, in.crt,
                         esp, rage, feedback),
          prediction_masks(in.storage, in.engine, in.configuration, history, scene, esp),
          player_receives(in.storage, in.players, fix),
          visual_receives(in.storage, in.configuration, in.players),
          player_menu(in.storage, in.engine), features(in.storage, in.configuration, history, skins,
                                                       rage, legit, esp, script_api, hooks),
          map_system(registry(in.storage, 0x43475B78u),
                     {in.storage.data().native_cell<std::int32_t*>(0x43468D0Cu),
                      in.storage.data().native_cell<std::int32_t*>(0x43467A70u),
                      in.storage.data().native_cell<void*>(0x43467204u),
                      in.storage.data().native_cell<void*>(0x4346A514u),
                      in.storage.owner_registries().at(0x43475770u)}),
          packets{in.storage.owner_registries().at(0x4347584Cu),
                  in.storage.data().native_cell<void*>(0x4346A514u),
                  in.storage.data().native_cell<void*>(0x4346A890u),
                  in.storage.data().native_cell<EntityCommandOwner*>(0x43475898u),
                  network::packet_dispatch_offsets(in.storage.offsets())},
          user_messages(in.storage, in.players.adjustments(), in.scripts.bridge()) {
        // Resolve construction cycles by composing the real contexts, before
        // any registered receiver can observe them. No feature factory runs.
        effects.compose_command(camera, in.players, scene, in.crt, {in.storage.data()});
        effects.compose_primary(camera, history, bones, in.crt);
        hooks.initialize_network(miscellaneous);
        hooks.initialize_callbacks(fix);
        hooks.initialize_player_auxiliary(in.engine, in.configuration, history, bones);
        commands.complete(hooks, local, target_refresh, shared_esp);
        player_hooks = std::make_unique<engine::PlayerHookFamily>(
            hooks.descriptor_storage().at(engine::local_player_descriptor_identity),
            hooks.descriptor_storage().at(engine::remote_player_descriptor_identity),
            hooks.registry(),
            in.storage.data().native_cell<std::byte*>(engine::player_primary_module_identity),
            hooks.descriptor_images(), hooks.descriptor_system(),
            engine::PlayerPrimaryHookRuntimes{
                fix.animations(), commands.runtime(), local.simulation(),
                hooks.player_auxiliary().free_look_capture(), hooks.player_auxiliary().visibility(),
                local.queries(), local.physics(), local.local_hooks(), local.command_hooks()});
        visual.initialize_game_system(in.shutdown.finalizer());
        script_api.bind_modules(in.presets, in.main_window);
        combat = std::make_unique<CombatRuntimeOwner>(in.storage, packet, tick_shift, anti_aim,
                                                      effects, in.scripts);
        world =
            std::make_unique<WorldRuntimeOwner>(in.storage, in.engine, in.configuration, history,
                                                visual, effects, hooks, in.window, in.presets);
        simple_hud = std::make_unique<hud::SimpleHudRuntimeOwner>(in.storage, in.configuration,
                                                                  visual, hooks, in.scripts);
        graph = std::make_unique<NativeStartupSourceGraph>(
            NativeStartupSourceGraph{in.storage.configuration(),
                                     in.storage.menu_hotkey(),
                                     in.configuration.movement(),
                                     in.configuration.skins_settings(),
                                     in.configuration.anti_aim_initializers(),
                                     in.crt.exit_registration(),
                                     in.configuration.static_initializers(),
                                     in.storage.data(),
                                     in.final_initializer,
                                     in.crt.first_startup_services(),
                                     in.crt.descriptor_initializers(),
                                     in.scripts.startup_initializers(),
                                     in.animation_cleanup,
                                     in.crt.group_initializers(),
                                     prediction_history.factory_bindings(),
                                     entity_commands.factory_bindings(),
                                     free_look.initializers(),
                                     target_mask.factory_bindings(),
                                     shared_esp.initializers(),
                                     feedback.factory_bindings(),
                                     buffered_shots.factory_bindings(),
                                     viewmodel.factory_bindings(),
                                     in.configuration.global_initializers(),
                                     simple_hud->context(),
                                     in.crt.flag_initializers(),
                                     in.crt.ui_type_initializers(),
                                     features.factory_bindings(),
                                     prediction_masks.factory_bindings(),
                                     map_system,
                                     miscellaneous.factory_bindings(),
                                     combat->factory_bindings(),
                                     skins.factory_bindings(),
                                     movement.factory_bindings(),
                                     history.factory_bindings(),
                                     world->factory_bindings(),
                                     world->proxy_factory_bindings(),
                                     in.static_crt.static_initializers(),
                                     in.crt.event_name_initializers(),
                                     in.static_crt.native_vector_cleanup(),
                                     in.static_crt.native_container_cleanup(),
                                     history.source_terminal_cleanup()});
    }
};

SourceFeatureRuntime::SourceFeatureRuntime(SourceFeatureInputs inputs)
    : SourceFeatureRuntime(inputs, native_imports(inputs)) {}
SourceFeatureRuntime::SourceFeatureRuntime(SourceFeatureInputs inputs,
                                           const SourceFeatureRuntimeImports& imports)
    : data_(std::make_unique<Data>(inputs, imports)) {}
SourceFeatureRuntime::~SourceFeatureRuntime() = default;
NativeStartupSourceGraph& SourceFeatureRuntime::graph() noexcept {
    return *data_->graph;
}
void SourceFeatureRuntime::bind_factories(NativeStartupSources& sources) {
    bind_runtime_feature_sources(sources, graph());
}
void SourceFeatureRuntime::initialize_features(NativeStartupSources& sources) {
    auto& d = *data_;
    if (d.factories_started)
        throw std::logic_error("Source feature initialization has already started");
    bind_factories(sources);
    d.factories_started = true;
    initialize_runtime_owners(sources);
    d.inputs.scripts.bind_manager();
}
void SourceFeatureRuntime::bind_hook_contexts() {
    auto& d = *data_;
    if (d.hook_contexts_bound)
        throw std::logic_error("Source hook contexts have already been bound");
    if (d.inputs.storage.owner_registries().at(0x43475B78u).entries().size() != 57)
        throw std::logic_error("Source hook contexts require all57 actual feature owners");
    auto* particles =
        d.inputs.storage.data().native_cell<effects::HarmlessMolotovRuntime*>(0x43475828u);
    if (!particles || &particles->bindings != &d.world->molotov_bindings())
        throw std::logic_error(
            "Source particle hook context requires its real constructed feature owner");
    // Client-state handlers borrow the actual ScriptOwner and RootMenu.
    // Both are created after the physical feature contexts assembled above.
    auto& in = d.inputs;
    d.hooks.initialize_client_state(in.engine, in.configuration, d.history, d.capture, in.scripts,
                                    in.main_window);
    const engine::BuildHookFamilies families{*d.player_hooks,
                                             d.fix.animations(),
                                             d.bones.remote_setup_bones(),
                                             d.hooks.callbacks(),
                                             d.hooks.player_auxiliary().ragdolls(),
                                             *particles,
                                             d.packets,
                                             d.hooks.channel(),
                                             d.hooks.messages(),
                                             d.viewmodel.hooks(),
                                             d.hooks.client_state().family(),
                                             d.effects.model_hooks(),
                                             d.hooks.auto_accept_hooks(),
                                             d.hooks.console_input()};
    engine::bind_build_hook_runtime_contexts(d.hooks.descriptor_storage(), d.hooks.registry(),
                                             families);
    d.user_messages.bind();
    d.hook_contexts_bound = true;
}
ui::RootMenu& SourceFeatureRuntime::compose_menu() {
    auto& d = *data_;
    auto& in = d.inputs;
    return in.main_window.compose({in.configuration.widget_identities(), in.crt.rage_groups(),
                                   d.camera.runtime(), d.miscellaneous.runtime(),
                                   d.settings.runtime(), d.skins.catalog(), d.skins.refresh(),
                                   in.players.adjustments(), d.player_menu.bindings(), in.presets,
                                   in.scripts.require_owner(), in.assets.icon_cache()},
                                  in.renderer, in.engine.variables);
}
engine::HookRuntimeOwner& SourceFeatureRuntime::hooks() noexcept {
    return data_->hooks;
}
engine::ClientClassFactoryRuntime& SourceFeatureRuntime::class_factories() noexcept {
    return data_->class_factories;
}
HistoryRuntimeOwner& SourceFeatureRuntime::history() noexcept {
    return data_->history;
}
LocalAnimationRuntimeOwner& SourceFeatureRuntime::local_animation() noexcept {
    return data_->local;
}
EntityCommandRuntimeOwner& SourceFeatureRuntime::entity_commands() noexcept {
    return data_->entity_commands;
}
CommandRuntimeOwner& SourceFeatureRuntime::commands() noexcept {
    return data_->commands;
}
VisualRuntimeOwner& SourceFeatureRuntime::visual() noexcept {
    return data_->visual;
}
MiscRuntimeOwner& SourceFeatureRuntime::miscellaneous() noexcept {
    return data_->miscellaneous;
}
WorldRuntimeOwner& SourceFeatureRuntime::world() noexcept {
    return *data_->world;
}
skins::SkinsRuntimeOwner& SourceFeatureRuntime::skins() noexcept {
    return data_->skins;
}
misc::SettingsRuntimeOwner& SourceFeatureRuntime::menu_settings() noexcept {
    return data_->settings;
}
misc::ViewmodelRuntimeOwner& SourceFeatureRuntime::viewmodel() noexcept {
    return data_->viewmodel;
}
effects::CameraRuntimeOwner& SourceFeatureRuntime::camera() noexcept {
    return data_->camera;
}
PlayerReceiveOwner& SourceFeatureRuntime::player_receives() noexcept {
    return data_->player_receives;
}
effects::VisualReceiveOwner& SourceFeatureRuntime::visual_receives() noexcept {
    return data_->visual_receives;
}
} //namespace skeet::client
