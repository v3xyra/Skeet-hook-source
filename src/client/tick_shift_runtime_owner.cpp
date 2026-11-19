#include "skeet/client/tick_shift_runtime_owner.hpp"
#include "skeet/client/combat_runtime_factories.hpp"
#include "skeet/client/command_capture_runtime_owner.hpp"
#include "skeet/client/command_checksum.hpp"
#include "skeet/client/command_runtime_owner.hpp"
#include "skeet/client/generated_command_owners.hpp"
#include "skeet/client/history_runtime_owner.hpp"
#include "skeet/client/local_animation_runtime_owner.hpp"
#include "skeet/client/movement_hud_runtime_owner.hpp"
#include "skeet/client/native_startup.hpp"
#include "skeet/client/player_runtime_owner.hpp"
#include "skeet/client/runtime_composition.hpp"
#include "skeet/client/runtime_storage.hpp"
#include "skeet/client/visual_runtime_owner.hpp"
#include "skeet/engine/decoded_offset.hpp"
#include "skeet/engine/netvar_initialization.hpp"
#include "skeet/engine/scene_runtime_owner.hpp"
#include "skeet/network/tick_shift_phase1.hpp"
#include "skeet/network/tick_shift_phase7.hpp"
#include "skeet/movement/command_wrapper.hpp"
#include "skeet/movement/rage_stop.hpp"
#include "skeet/script/source_runtime_owner.hpp"
#include <new>
#include <stdexcept>

namespace skeet::client {
namespace {
std::ptrdiff_t offset(ClientRuntimeStorage& s, std::uint32_t identity) {
    const auto& g = s.offsets().globals;
    for (const auto& row : engine::build_receive_offset_recipes())
        if (row.pointer_identity == identity)
            return engine::DecodedOffset(g, identity, row.xor_key_identity, row.initial_xor_key, 0);
    for (const auto& row : engine::build_data_map_offset_recipes())
        if (row.pointer_identity == identity)
            return engine::DecodedOffset(g, identity, row.xor_key_identity, row.initial_xor_key, 0);
    throw std::invalid_argument("Tick-shift field has no recovered receive/datamap recipe");
}
ClientRuntimeStorage& checked(ClientRuntimeStorage& s, const EngineRuntimeBindings& e,
                              ConfigurationRuntimeOwner& c, HistoryRuntimeOwner& h,
                              MovementHudRuntimeOwner& m, CommandRuntimeOwner& command,
                              LocalAnimationRuntimeOwner& local,
                              engine::SceneQueryRuntimeOwner& scene, VisualRuntimeOwner& visual,
                              script::SourceScriptRuntimeOwner& scripts) {
    if (&e.variables.data() != &s.data() || &c.storage() != &s || &h.storage() != &s ||
        &m.storage() != &s || &command.storage() != &s || &command.history() != &h ||
        &local.storage() != &s || &local.history() != &h || &scene.storage() != &s ||
        &scene.players() != &h.players() || &visual.storage() != &s || &scripts.storage() != &s)
        throw std::invalid_argument(
            "Tick shifting requires canonical command, history, movement, local-animation, scene, HUD and Lua owners");
    auto& data = s.data();
    if (!e.command.seed_generator || !e.command.random_seed ||
        e.command.seed_generator != data.native_cell<prediction::SeedGenerator>(0x434679ACu) ||
        e.command.random_seed != data.native_cell<std::int32_t*>(0x43468328u))
        throw std::invalid_argument("Tick shifting requires the acquired genuine command seed inputs");
    for (const auto identity : {0x434757DCu, 0x43475B2Cu, 0x43475BA4u, 0x4346A338u, 0x43467EB8u,
                                0x4346A334u, 0x43475A60u, 0x4346A894u, 0x43469264u})
        if (!data.native_cell<void*>(identity))
            throw std::invalid_argument("Tick-shift command/clock producer is missing");
    if (!e.variables.at(0x4346A850u).get())
        throw std::invalid_argument("Tick-shift acknowledgement ConVar is not acquired");
    initialize_source_command_checksum(data);
    return s;
}
engine::OwnerRegistryBinding registry(ClientRuntimeStorage& storage, std::uint32_t identity) {
    return {storage.owner_registries().at(identity), storage.memory()};
}
}
struct TickShiftRuntimeOwner::Data {
    ClientRuntimeStorage& storage;
    InputBitmap input;
    network::TickShiftState state{};
    network::TickShiftRuntimeState runtime;
    network::TickShiftPhaseState phases{};
    network::TickShiftControlView controls;
    network::TickShiftRuntimeSettings settings;
    rage::TargetBindings targeting;
    GeneratedCommandOwnersBindings generated;
    RageStopBindings stopping;
    network::TickShiftRuntimeBindings live;
    network::TickShiftPhase1Bindings first;
    network::TickShiftPhase7Bindings last;
    network::TickShiftCommandOwner command;
    network::TickShiftAcknowledgementBindings acknowledgement;
    network::TickShiftMessageBindings messages;
    hud::TickShiftIndicatorBindings indicator;
    network::TickShiftOwnerHudBindings hud;
    TickShiftRuntimeFactoryBindings factory;
    static void initialize(void* raw) {
        auto& self = *static_cast<Data*>(raw);
        auto* allocation = network::allocate_tick_shift_owner_storage(self.storage.memory());
        //4347FF44 reaches unresolved INT1/junk. This explicit source failure
        //policy neither constructs nor registers an invented null owner.
        if (!allocation)
            throw std::bad_alloc();
        network::construct_allocated_tick_shift_owner(
            allocation, self.factory.registries, self.storage.configuration(), self.factory.owner);
    }
    Data(ClientRuntimeStorage& s, const EngineRuntimeBindings& e, ConfigurationRuntimeOwner& c,
         HistoryRuntimeOwner& h, MovementHudRuntimeOwner& m, CommandRuntimeOwner& core,
         LocalAnimationRuntimeOwner& local, engine::SceneQueryRuntimeOwner& scene,
         VisualRuntimeOwner& visual, script::SourceScriptRuntimeOwner& scripts)
        : storage(checked(s, e, c, h, m, core, local, scene, visual, scripts)),
          input(s.input_runtime().bitmap()), runtime{state, 0, 0, 0, 0, 0, {}, {}, {}},
          controls{c.rage_settings().tick_shift, input},
          settings{controls,
                   c.rage_settings().global.duck_peek_assist.value.binding,
                   s.data().native_cell<std::uint8_t>(0x43467560u),
                   s.data().native_cell<std::int32_t>(0x43469274u),
                   s.data().native_cell<std::int32_t>(0x43468290u),
                   s.data().native_cell<std::int32_t>(0x4346DB30u)},
          targeting{s.data().native_cell<void*>(0x43475808u),
                    s.data().native_cell<void*>(0x4346A890u),
                    h.game_type(),
                    h.teammates_are_enemies(),
                    h.players().adjustments(),
                    h.target_offsets()},
          generated{s.data().native_cell<MovementCommandOwner*>(0x434759B0u),
                    s.data().native_cell<anti_aim::CommandOwner*>(0x43475940u),
                    m.movement_command().bindings.command, c.movement().infinite_duck.value,
                    std::uint32_t(offset(s, 0x4346A6DCu))},
          stopping{m.movement_environment()},
          live{s.data().native_cell<void*>(0x4346A514u),
               s.data().native_cell<void**>(0x4346A3A4u),
               s.data().native_cell<const void*>(0x43475B74u),
               s.data().native_cell<const void*>(0x43475808u),
               s.data().native_cell<std::int32_t*>(0x434757DCu),
               s.data().native_cell<const std::int32_t*>(0x43475B2Cu),
               s.data().native_cell<const std::int32_t*>(0x43475BA4u),
               s.data().native_cell<const float*>(0x4346A338u),
               s.data().native_cell<float*>(0x43467EB8u),
               {},
               core.capture().firing(),
               {offset(s, 0x43468378u), offset(s, 0x4346836Cu), offset(s, 0x43468BF4u),
                offset(s, 0x43468E78u), offset(s, 0x4346A438u), offset(s, 0x434693D8u),
                offset(s, 0x434670ECu), offset(s, 0x43467AA4u), offset(s, 0x43467A6Cu)},
               s.data().native_cell<std::array<std::uint32_t, 3>>(0x43474580u),
               {s.data().native_cell<void*>(0x4346A894u), 0xF0},
               {s.data().native_cell<void*>(0x4346A894u), 0xF4},
               s.data().native_cell<CommandChecksum>(0x43467C48u),
               s.data().native_cell<PredictionHistoryOwner*>(0x434759B4u),
               s.data().native_cell<EntityCommandOwner*>(0x43475898u),
               s.recharge_observers(),
               s.data().native_cell<const std::uint32_t*>(0x4346A334u),
               s.data().native_cell<void*>(0x43469264u)},
          first{c.rage_settings().global,
                s.data().native_cell<void*>(0x4346A890u),
                offset(s, 0x43467A74u),
                local.restricted().offsets.field_2a671b42,
                s.data().native_cell<void*>(0x43475AACu),
                *s.data().native_cell<std::int32_t*>(0x43475A60u),
                s.data().native_cell<std::int32_t>(0x4346DB1Cu),
                s.data().native_cell<std::int32_t>(0x4346DB2Cu),
                s.data().native_cell<std::array<float, 3>>(0x4346DB20u),
                s.recharge_observers()},
          last{core.capture(),
               core.capture().capture_offsets(),
               core.prediction(),
               core.prediction_player_offsets(),
               s.prediction_observers(),
               s.recharge_observers(),
               s.data().native_cell<prediction::SeedGenerator>(0x434679ACu),
               s.data().native_cell<std::int32_t*>(0x43468328u),
               scripts,
               generated,
               s.data().native_cell<std::array<std::uint32_t, 150>>(0x43476090u),
               c.anti_aim_other(),
               core.trigonometry(),
               core.capture().angle_environment(),
               m.movement_capture(),
               m.movement_environment(),
               stopping,
               scene.queries().movement(),
               targeting,
               s.data().native_cell<anti_aim::TargetState>(0x4346DB14u),
               c.rage_settings().global.quick_stop.value,
               c.rage_settings().global.activation.value.binding,
               input,
               reinterpret_cast<const std::uint32_t&>(
                   c.rage_settings().global.double_tap_quick_stop.value),
               s.data().native_cell<std::uint32_t>(0x43468488u),
               s.data().native_cell<std::int32_t>(0x434759F4u),
               s.data().native_cell<std::int32_t>(0x434759C4u),
               s.data().native_cell<PredictionHistoryOwner*>(0x434759B4u),
               local.restricted(),
               s.data().native_cell<std::uint8_t*>(0x434683D0u),
               {offset(s, 0x43468308u), offset(s, 0x43467554u), offset(s, 0x43467184u),
                offset(s, 0x4346A828u), offset(s, 0x434688ACu), offset(s, 0x4346720Cu)}},
          command{runtime,
                  settings,
                  live,
                  s.data().native_cell<std::uint32_t>(0x434759C0u),
                  {s.data().native_cell<network::LatencySamples>(0x43478C70u),
                   h.entity_runtime().command_tick,
                   s.data().native_cell<const std::int32_t*>(0x43468F9Cu),
                   s.data().native_cell<const std::int32_t*>(0x434670A8u)},
                  phases,
                  first,
                  last,
                  0,
                  controls},
          acknowledgement{s.data().native_cell<void*>(0x4346A514u),
                          s.data().native_cell<void*>(0x43467B04u),
                          s.data().native_cell<std::int32_t*>(0x43475BA4u),
                          s.data().native_cell<const std::int32_t*>(0x43475794u),
                          s.data().native_cell<float*>(0x4346A338u),
                          s.data().native_cell<std::int32_t>(0x43468290u),
                          e.variables.at(0x4346A850u),
                          std::uint32_t(offset(s, 0x43468E78u)),
                          std::uint32_t(offset(s, 0x434693D8u))},
          messages{s.data().native_cell<void*>(0x434683E0u),
                   s.data().native_cell<void*>(0x4346A894u),
                   s.data().native_cell<std::int32_t*>(0x434757DCu),
                   s.data().native_cell<std::int32_t*>(0x43475B2Cu),
                   s.data().native_cell<void**>(0x43475B0Cu)},
          indicator{s.data().native_cell<void*>(0x4346A890u), std::uint32_t(offset(s, 0x43468E78u)),
                    std::uint32_t(offset(s, 0x43467A74u)), std::uint32_t(offset(s, 0x4346908Cu))},
          hud{scripts.bridge(),
              s.data().native_cell<float>(0x43475890u),
              c.other_esp_settings().feature_indicators,
              indicator,
              *reinterpret_cast<float*>(s.data().native_cell<std::byte*>(0x43467B04u) + 0x14),
              visual.indicators()},
          factory{{registry(s, 0x43475B78u), registry(s, 0x43475A9Cu), registry(s, 0x43475B94u),
                   s.command_phases(), s.data_update_observers(), s.prediction_observers()},
                  {command, acknowledgement, messages, hud}} {}
};
TickShiftRuntimeOwner::TickShiftRuntimeOwner(ClientRuntimeStorage& s, const EngineRuntimeBindings& e,
                                         ConfigurationRuntimeOwner& c, HistoryRuntimeOwner& h,
                                         MovementHudRuntimeOwner& m, CommandRuntimeOwner& core,
                                         LocalAnimationRuntimeOwner& local,
                                         engine::SceneQueryRuntimeOwner& scene,
                                         VisualRuntimeOwner& visual,
                                         script::SourceScriptRuntimeOwner& scripts)
    : data_(std::make_unique<Data>(s, e, c, h, m, core, local, scene, visual, scripts)) {}
TickShiftRuntimeOwner::~TickShiftRuntimeOwner() = default;
ClientRuntimeStorage& TickShiftRuntimeOwner::storage() const noexcept {
    return data_->storage;
}
network::TickShiftCommandOwner& TickShiftRuntimeOwner::command() noexcept {
    return data_->command;
}
network::TickShiftState& TickShiftRuntimeOwner::state() noexcept {
    return data_->state;
}
TickShiftRuntimeFactoryBindings& TickShiftRuntimeOwner::factory_bindings() noexcept {
    return data_->factory;
}
void TickShiftRuntimeOwner::bind_initializers(NativeStartupSources& sources) {
    if (&sources.data() != &storage().data())
        throw std::invalid_argument("Tick-shift factory requires the canonical startup dispatcher");
    sources.bind_initializer({tick_shift_runtime_factory, data_.get(), Data::initialize});
}
} //namespace skeet::client
