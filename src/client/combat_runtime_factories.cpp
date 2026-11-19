#include "skeet/client/combat_runtime_factories.hpp"
#include "skeet/client/native_startup.hpp"
#include "skeet/engine/owner_allocation.hpp"
#include "skeet/network/tick_shift_phase7.hpp"
#include <algorithm>
#include <array>
#include <new>
#include <stdexcept>

namespace skeet::client {
namespace {
void* allocate(engine::JobMemory memory, std::size_t native_size, std::size_t source_size) {
    return engine::allocate_owner_storage(memory, std::max(native_size, source_size));
}
void require_unlinked(const ConfigDescriptor& descriptor) {
    if (descriptor.next || descriptor.previous)
        throw std::invalid_argument("Combat factory configuration was already registered");
}
void require_packet_state(const PacketRuntimeFactoryBindings& source) {
    const auto& state = source.command.state;
    if (&source.command.settings != &source.settings ||
        &state.mode != &source.settings.amount.value ||
        &state.variance != &source.settings.variance.value ||
        &state.configured_cap != &source.settings.limit.value)
        throw std::invalid_argument("Packet factory must share its registered fake-lag controls");
    require_unlinked(source.settings.amount);
    require_unlinked(source.settings.variance);
    require_unlinked(source.settings.limit);
}
void require_tick_shift_state(const TickShiftRuntimeFactoryBindings& source) {
    const auto& command = source.owner.command;
    if (&command.controls != &command.settings.controls)
        throw std::invalid_argument("Tick-shift factory must share its canonical runtime controls");
    const auto& config = command.controls.settings;
    for (const auto* descriptor : std::array<const ConfigDescriptor*, 8>{
             &config.on_shot_anti_aim, &config.on_shot_key, &config.double_tap_fake_lag_limit,
             &config.fake_peek, &config.fake_peek_key, &config.double_tap, &config.double_tap_key,
             &config.double_tap_mode})
        require_unlinked(*descriptor);
}
void require_anti_aim_state(const AntiAimRuntimeFactoryBindings& source) {
    const auto& command = source.owner.command;
    if (&command.precalculation.common != static_cast<const anti_aim::State*>(&command.state) ||
        &command.finisher.common != &command.state || command.finisher.matrices_520.size() != 256u)
        throw std::invalid_argument(
            "Anti-aim factory requires its canonical state and 256 bone matrices");
    const auto& config = command.config;
    for (const auto* descriptor : std::array<const ConfigDescriptor*, 11>{
             &config.yaw_base, &config.yaw_jitter, &config.yaw_jitter_offset, &config.edge_yaw,
             &config.freestanding, &config.body_yaw_offsets, &config.yaw_offsets,
             &config.freestanding_hotkey, &config.freestanding_body_yaw, &config.roll,
             &config.custom_pitch})
        require_unlinked(*descriptor);
}
void require_effects_state(const EffectsRuntimeFactoryBindings& source) {
    const auto& owner = source.owner;
    const auto& command = owner.command;
    const auto& lifecycle = owner.lifecycle;
    const auto& updates = owner.updates;
    if (&lifecycle.models != &command.models || &lifecycle.world != &command.world ||
        &lifecycle.colors != &command.models.settings || &updates.models != &command.models ||
        &updates.world != &command.world || &updates.lifecycle != &lifecycle.state ||
        &updates.view.settings != &command.world.settings ||
        &command.camera.force_dead != &command.world.settings.third_person_dead.value ||
        &command.camera.alive_hotkey != &command.world.settings.third_person_key.value)
        throw std::invalid_argument(
            "Effects factory requires its canonical camera, world and model views");
    if (command.models.solid_material || command.models.shadow_material ||
        command.models.base_texture || command.models.metallic_environment ||
        command.models.glow_environment || command.models.fullscreen_texture ||
        lifecycle.state.allocation_760)
        throw std::invalid_argument("Effects factory requires its unallocated resource state");
    for (const auto& material : command.models.materials)
        if (material.material)
            throw std::invalid_argument("Effects materials were already initialized");
    // These owner descriptors must be fresh. Independent CRT descriptors in
    // EffectsSettings/ColoredModelsSettings are deliberately left untouched.
    const auto& effects = command.world.settings;
    for (const auto* descriptor : std::array<const ConfigDescriptor*, 10>{
             &effects.impact_duration, &effects.wall_transparency, &effects.prop_transparency,
             &effects.brightness, &effects.night_color, &effects.remove_fog,
             &effects.disable_post_processing, &effects.instant_scope, &effects.bullet_impacts,
             &effects.remove_sky})
        require_unlinked(*descriptor);
    const auto& colors = lifecycle.colors;
    for (const auto* group : std::array<const esp::ModelColorGroup*, 8>{
             &colors.player, &colors.teammate, &colors.local_player, &colors.local_fake,
             &colors.on_shot, &colors.hands, &colors.weapon_viewmodel, &colors.weapons}) {
        require_unlinked(group->enabled);
        require_unlinked(group->primary_color);
        require_unlinked(group->material);
        require_unlinked(group->secondary_color);
    }
    for (const auto* descriptor : std::array<const ConfigDescriptor*, 10>{
             &colors.props, &colors.props_color, &colors.player.behind_wall,
             &colors.player.behind_wall_color, &colors.teammate.behind_wall,
             &colors.teammate.behind_wall_color, &colors.local_transparency,
             &colors.on_shot_duration, &colors.shadow, &colors.shadow_color})
        require_unlinked(*descriptor);
}
void zero_packet_state(network::FakeLagState& state) {
    // HEAP_ZERO_MEMORY covers these owner words. The leading references keep
    // pointing to the same three independently registered descriptor payloads.
    state.flags = 0;
    state.target_trigger_time = 0.0f;
    state.cycle = 0;
    state.target = 0;
    state.selected = 0;
    state.recorded_choked = 0;
    state.value_58 = 0;
    state.last_trigger_time = 0.0f;
    state.tracked_target = 0;
    state.previous_selected = 0;
    state.last_origin = {};
    state.saved_duck_amount_bits = 0;
    state.previous_speed_delta = 0.0f;
    state.value_7c = 0;
    state.acceleration = {};
    state.value_8c = 0;
}
void zero_anti_aim_state(anti_aim::CommandOwner& command) {
    // Same fresh-allocation projection as anti_aim::create_owner; this caller
    // reserves the native3520-byte minimum before using the complete constructor.
    command.config = anti_aim::Config{};
    command.state = anti_aim::AngleStageState{};
    command.state.cached_yaw_18c = 0.0f;
    command.parameters = {0, 0, 0, 0, 0, 0, 0, 0, 0, {}};
    command.precalculation.feet_yaw_194 = 0.0f;
    command.precalculation.micro_move_19c = 0.0f;
    command.precalculation.previous_duck_1a4 = 0.0f;
    std::fill(command.finisher.animation_1d8.begin(), command.finisher.animation_1d8.end(),
              std::byte{0});
    command.finisher.absolute_angles_1b8 = {};
    std::fill(command.finisher.matrices_520.begin(), command.finisher.matrices_520.end(),
              Matrix3x4{});
}
void zero_effects_values(EffectsRuntimeFactoryBindings& source) {
    auto& effects = source.owner.command.world.settings;
    effects.brightness.value = 0;
    for (auto* value : std::array<BooleanConfig*, 5>{
             &effects.remove_fog, &effects.disable_post_processing, &effects.instant_scope,
             &effects.bullet_impacts, &effects.remove_sky})
        value->value = 0;
    auto& colors = source.owner.lifecycle.colors;
    for (auto* group : std::array<esp::ModelColorGroup*, 8>{
             &colors.player, &colors.teammate, &colors.local_player, &colors.local_fake,
             &colors.on_shot, &colors.hands, &colors.weapon_viewmodel, &colors.weapons})
        group->enabled.value = 0;
    colors.props.value = 0;
    colors.player.behind_wall.value = 0;
    colors.teammate.behind_wall.value = 0;
    colors.shadow.value = 0;
}
void packet_entry(void* context) {
    create_packet_runtime_owner(*static_cast<CombatRuntimeFactoryBindings*>(context));
}
void tick_shift_entry(void* context) {
    create_tick_shift_runtime_owner(*static_cast<CombatRuntimeFactoryBindings*>(context));
}
void anti_aim_entry(void* context) {
    create_anti_aim_runtime_owner(*static_cast<CombatRuntimeFactoryBindings*>(context));
}
void effects_entry(void* context) {
    create_effects_runtime_owner(*static_cast<CombatRuntimeFactoryBindings*>(context));
}
void script_entry(void* context) {
    create_script_runtime_owner(*static_cast<CombatRuntimeFactoryBindings*>(context));
}
}

void* create_packet_runtime_owner(CombatRuntimeFactoryBindings& source) {
    return create_packet_runtime_owner(source.packet, source.configuration);
}
void* create_packet_runtime_owner(PacketRuntimeFactoryBindings& packet,
                                  ConfigRegistries& configuration) {
    require_packet_state(packet);
    auto* storage = allocate(packet.lifecycle.memory, 0x98u, network::packet_owner_size());
    // Native null edge4347DA99 is INT1 followed by unresolved junk. This is
    // the source allocation-failure policy, not a claimed native continuation.
    if (!storage)
        throw std::bad_alloc();
    zero_packet_state(packet.command.state);
    network::construct_packet_owner(storage, packet.lifecycle, packet.commands, configuration,
                                    packet.settings, packet.command);
    return storage;
}
void* create_tick_shift_runtime_owner(CombatRuntimeFactoryBindings& source) {
    auto& tick_shift = source.tick_shift;
    require_tick_shift_state(tick_shift);
    auto* storage =
        allocate(tick_shift.registries.primary.memory, network::native_tick_shift_owner_allocation_size,
                 network::tick_shift_owner_size());
    // Native null edge4347FF44 is INT1/junk; no owner is registered or state
    // projected on that path. Allocation failure is explicit in the source.
    if (!storage)
        throw std::bad_alloc();
    network::construct_allocated_tick_shift_owner(storage, tick_shift.registries, source.configuration,
                                               tick_shift.owner);
    return storage;
}
void* create_anti_aim_runtime_owner(CombatRuntimeFactoryBindings& source) {
    return create_anti_aim_runtime_owner(source.anti_aim, source.configuration);
}
void* create_anti_aim_runtime_owner(AntiAimRuntimeFactoryBindings& aa,
                                    ConfigRegistries& configuration) {
    require_anti_aim_state(aa);
    auto* storage = allocate(aa.registries.primary.memory, 0x3520u, anti_aim::owner_size());
    if (!storage) {
        aa.owner.active = nullptr;
        return nullptr;
    } //43485CE3..43485CE5
    zero_anti_aim_state(aa.owner.command);
    anti_aim::construct_owner(storage, aa.registries, configuration, aa.owner);
    aa.owner.active = &aa.owner.command; //43485CE5, canonical source view
    return storage;
}
void* create_effects_runtime_owner(CombatRuntimeFactoryBindings& source) {
    return create_effects_runtime_owner(source.effects, source.configuration);
}
void* create_effects_runtime_owner(EffectsRuntimeFactoryBindings& effects,
                                   ConfigRegistries& configuration) {
    require_effects_state(effects);
    auto& image = effects.owner.data;
    if (&configuration.descriptors != &image.native_cell<ConfigLink>(0x434798BCu) ||
        &configuration.hotkeys != &image.native_cell<HotkeyLink>(0x434798C4u) ||
        &effects.owner.active != &image.native_cell<effects::EffectsCommandOwner*>(0x43475A1Cu))
        throw std::invalid_argument(
            "Effects factory requires its canonical data, configuration and singleton cells");
    auto* storage =
        allocate(effects.registries.lifecycle.memory, 0x768u, effects::model_effects_owner_size());
    if (!storage) {
        effects.owner.active = nullptr;
        return nullptr;
    } //43484FAA..43484FAC
    zero_effects_values(effects);
    effects::construct_model_effects_owner(storage, effects.registries, configuration,
                                           effects.owner);
    // construct_model_effects_owner publishes the same source view on success.
    return storage;
}
script::ScriptOwner* create_script_runtime_owner(CombatRuntimeFactoryBindings& source) {
    return create_script_runtime_owner(source.scripting);
}
script::ScriptOwner* create_script_runtime_owner(ScriptRuntimeFactoryBindings& scripting) {
    script::ScriptModulesOwner::validate_construction(scripting.runtime, scripting.modules);
    if (!scripting.owner.system.add_handler || !scripting.owner.system.remove_handler ||
        !scripting.owner.system.set_bit || !scripting.owner.executable.bitmap.buffer)
        throw std::invalid_argument(
            "Script factory requires its existing exception imports and executable bitmap");
    const auto memory = scripting.owner.primary.memory;
    auto* storage = allocate(memory, 0x288u, sizeof(script::ScriptOwner));
    if (!storage) {
        scripting.owner.current_owner = nullptr;
        return nullptr;
    } //4348E61F..4348E621
    return &script::construct_script_owner({storage, memory}, scripting.runtime, scripting.owner,
                                           scripting.modules);
}

void bind_combat_runtime_factories(NativeStartupSources& startup,
                                   CombatRuntimeFactoryBindings& source) {
    const auto& image = engine::require_native_data_image(source.data);
    if (&image != &startup.data() || &source.registries.image() != &image ||
        &source.configuration.descriptors != &image.native_cell<ConfigLink>(0x434798BCu) ||
        &source.configuration.hotkeys != &image.native_cell<HotkeyLink>(0x434798C4u))
        throw std::invalid_argument(
            "Combat factories require the common startup image and configuration sentinels");
    const auto memory = source.packet.lifecycle.memory;
    if (!memory.allocate || !memory.free || !memory.process_heap)
        throw std::invalid_argument("Combat factories require the canonical process heap imports");
    const auto registry = [&](engine::OwnerRegistryBinding binding, std::uint32_t address) {
        if (&binding.registry != &source.registries.at(address) ||
            binding.memory.allocate != memory.allocate || binding.memory.free != memory.free ||
            binding.memory.process_heap != memory.process_heap)
            throw std::invalid_argument(
                "Combat factories must share their native registry cells and heap");
    };
    auto& packet = source.packet;
    auto& tick_shift = source.tick_shift;
    auto& aa = source.anti_aim;
    auto& effects = source.effects;
    auto& scripting = source.scripting;
    auto& modules = scripting.modules;
    registry(packet.lifecycle, 0x43475B78u);
    registry(tick_shift.registries.primary, 0x43475B78u);
    registry(tick_shift.registries.hud, 0x43475A9Cu);
    registry(tick_shift.registries.messages, 0x43475B94u);
    registry(aa.registries.primary, 0x43475B78u);
    registry(aa.registries.hud, 0x43475A9Cu);
    registry(aa.registries.frames, 0x43475AD8u);
    registry(effects.registries.lifecycle, 0x43475B78u);
    registry(effects.registries.hud, 0x43475A9Cu);
    registry(effects.registries.updates, 0x43475988u);
    registry(effects.registries.frames, 0x43475AD8u);
    registry(scripting.owner.primary, 0x43475B78u);
    registry(modules.hud_callbacks, 0x43475A9Cu);
    registry(modules.network_callbacks, 0x43475B94u);
    registry(modules.frame_callbacks, 0x43475AD8u);
    registry(modules.update_callbacks, 0x43475988u);
    registry(modules.event_callbacks, 0x434758E0u);
    if (&packet.commands.image() != &image || &tick_shift.registries.commands != &packet.commands ||
        &aa.registries.commands != &packet.commands ||
        &effects.registries.commands != &packet.commands ||
        &modules.command_callbacks != &packet.commands ||
        &effects.registries.entities != &modules.entity_callbacks ||
        &effects.registries.entities.image() != &image ||
        &effects.registries.empty_ticks.image() != &image ||
        &tick_shift.registries.data_updates.image() != &image ||
        &tick_shift.registries.prediction.image() != &image ||
        &modules.prediction_callbacks != &tick_shift.registries.prediction ||
        &modules.config.configuration != &source.configuration)
        throw std::invalid_argument(
            "Combat factories require shared typed registries and module configuration");
    if (&aa.owner.active != &image.native_cell<anti_aim::CommandOwner*>(0x43475940u) ||
        &effects.owner.active != &image.native_cell<effects::EffectsCommandOwner*>(0x43475A1Cu) ||
        &scripting.owner.current_owner != &image.native_cell<script::ScriptOwner*>(0x43475B00u))
        throw std::invalid_argument("Combat factories require their canonical singleton cells");
    if (&packet.command.target != &aa.owner.command.globals.target ||
        &tick_shift.owner.command.phase7.target != &packet.command.target ||
        &aa.owner.command.globals.scheduler_flags != &packet.commands.services().scheduler_flags ||
        &tick_shift.owner.command.command_flags != &packet.commands.services().scheduler_flags ||
        &modules.command.scheduler_flags != &packet.commands.services().scheduler_flags ||
        &aa.owner.hud.scripts != &scripting.runtime.bridge ||
        &tick_shift.owner.hud.scripts != &scripting.runtime.bridge)
        throw std::invalid_argument(
            "Combat commands must share targeting, scheduler flags, Lua bridge and tick-shift controls");
    require_packet_state(packet);
    require_tick_shift_state(tick_shift);
    require_anti_aim_state(aa);
    require_effects_state(effects);
    startup.bind_initializer({packet_runtime_factory, &source, packet_entry});
    startup.bind_initializer({tick_shift_runtime_factory, &source, tick_shift_entry});
    startup.bind_initializer({anti_aim_runtime_factory, &source, anti_aim_entry});
    startup.bind_initializer({effects_runtime_factory, &source, effects_entry});
    startup.bind_initializer({script_runtime_factory, &source, script_entry});
}
} // namespace skeet::client
