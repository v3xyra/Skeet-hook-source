#include "skeet/prediction/observer_registry.hpp"
#include "skeet/prediction/player_simulation.hpp"
#include "skeet/client/prediction_history_owner.hpp"
#include "skeet/client/entity_command_owner.hpp"
#include "skeet/misc/quick_switch.hpp"
#include "skeet/misc/viewmodel_lag.hpp"
#include "skeet/network/tick_shift_runtime.hpp"
#include "skeet/script/bridge.hpp"
#include "skeet/engine/unload.hpp"
#include <stdexcept>

#if defined(_M_IX86) || defined(__i386__)
#define PREDICTION_OBSERVER_FAST __fastcall
#else
#define PREDICTION_OBSERVER_FAST
#endif

namespace skeet::prediction {
namespace {
constexpr std::array<ObserverTable, 6> tables{{
    {ObserverOwnerId::prediction_history, {0x4332C3C9u, 0x4340B612u, 0x433E9F3Bu, 0x433C95B7u}},
    {ObserverOwnerId::quick_switch, {0x4341F4A3u, 0x43400E0Du, 0x433E9F3Bu, 0x433C95B7u}},
    {ObserverOwnerId::tick_shift, {0x433C95B7u, 0x433C95B7u, 0x43328C18u, 0x433C95B7u}},
    {ObserverOwnerId::entity_commands, {0x4332BBA4u, 0x43382703u, 0x433E9F3Bu, 0x433B265Cu}},
    {ObserverOwnerId::script, {0x433F48F6u, 0x433C13C5u, 0x433E9F3Bu, 0x433C95B7u}},
    {ObserverOwnerId::viewmodel_lag, {0x4332EFD2u, 0x43367D42u, 0x433E9F3Bu, 0x4332B1EBu}},
}};
void PREDICTION_OBSERVER_FAST empty_command(NativePredictionObserverReceiver*, void*,
                                            const client::Command*) {} //433C95B7
void PREDICTION_OBSERVER_FAST empty_rejected(NativePredictionObserverReceiver*, void*,
                                             const PlayerCommandContext*) {} //433C95B7
std::int32_t PREDICTION_OBSERVER_FAST unchanged_tick(NativePredictionObserverReceiver*,
                                                     std::int32_t, std::int32_t tick) {
    return tick;
} //433E9F3B
void PREDICTION_OBSERVER_FAST capture_history(NativePredictionObserverReceiver* receiver,
                                              void* player, const client::Command* command) {
    client::begin_prediction_history(
        *static_cast<client::PredictionHistoryOwner*>(receiver->source), player, *command);
}
void PREDICTION_OBSERVER_FAST finish_history(NativePredictionObserverReceiver* receiver,
                                             void* player, const client::Command* command) {
    client::capture_prediction_history(
        *static_cast<client::PredictionHistoryOwner*>(receiver->source), player, *command);
}
void PREDICTION_OBSERVER_FAST capture_quick_switch(NativePredictionObserverReceiver* receiver,
                                                   void* player, const client::Command* command) {
    static_cast<misc::QuickSwitchRuntime*>(receiver->source)
        ->capture_before_prediction(player, *command);
}
void PREDICTION_OBSERVER_FAST finish_quick_switch(NativePredictionObserverReceiver* receiver,
                                                  void* player, const client::Command* command) {
    static_cast<misc::QuickSwitchRuntime*>(receiver->source)->finish_prediction(player, *command);
}
std::int32_t PREDICTION_OBSERVER_FAST tick_shift_tick(NativePredictionObserverReceiver* receiver,
                                                   std::int32_t number, std::int32_t tick) {
    //43328C18 reads the canonical full-owner+200 command/tick producer.
    const auto& stored =
        static_cast<network::TickShiftRuntimeState*>(receiver->source)->command_ticks[number % 150];
    return stored.command == number ? stored.tick : tick;
}
void PREDICTION_OBSERVER_FAST capture_entity_commands(NativePredictionObserverReceiver* receiver,
                                                      void* player,
                                                      const client::Command* command) {
    client::begin_entity_prediction(*static_cast<client::EntityCommandOwner*>(receiver->source),
                                    player, *command);
}
void PREDICTION_OBSERVER_FAST finish_entity_commands(NativePredictionObserverReceiver* receiver,
                                                     void* player, const client::Command* command) {
    client::finish_entity_prediction(*static_cast<client::EntityCommandOwner*>(receiver->source),
                                     player, *command);
}
void PREDICTION_OBSERVER_FAST reject_entity_commands(NativePredictionObserverReceiver* receiver,
                                                     void* player,
                                                     const PlayerCommandContext* context) {
    client::reject_entity_prediction(*static_cast<client::EntityCommandOwner*>(receiver->source),
                                     player, *context);
}
void PREDICTION_OBSERVER_FAST capture_script(NativePredictionObserverReceiver* receiver, void*,
                                             const client::Command* command) {
    auto& owner = *static_cast<ScriptPredictionObserver*>(receiver->source);
    if (owner.bridge.state) {
        const auto* event = owner.before_event.get();
        const auto number = command->number;
        script::dispatch_integer_event(owner.bridge, event, owner.command_number_field, number);
    }
}
void PREDICTION_OBSERVER_FAST finish_script(NativePredictionObserverReceiver* receiver, void*,
                                            const client::Command* command) {
    auto& owner = *static_cast<ScriptPredictionObserver*>(receiver->source);
    if (owner.bridge.state) {
        const auto* event = owner.after_event.get();
        const auto number = command->number;
        script::dispatch_integer_event(owner.bridge, event, owner.command_number_field, number);
    }
}
void PREDICTION_OBSERVER_FAST capture_viewmodel(NativePredictionObserverReceiver* receiver,
                                                void* player, const client::Command* command) {
    auto& owner = *static_cast<ViewmodelPredictionObserver*>(receiver->source);
    misc::prepare_viewmodel_prediction(owner.state, owner.bindings, player, *command);
}
void PREDICTION_OBSERVER_FAST finish_viewmodel(NativePredictionObserverReceiver* receiver,
                                               void* player, const client::Command* command) {
    auto& owner = *static_cast<ViewmodelPredictionObserver*>(receiver->source);
    misc::capture_viewmodel_prediction(owner.state, owner.bindings, player, *command);
}
void PREDICTION_OBSERVER_FAST reject_viewmodel(NativePredictionObserverReceiver* receiver, void*,
                                               const PlayerCommandContext* context) {
    auto& owner = *static_cast<ViewmodelPredictionObserver*>(receiver->source);
    misc::copy_previous_viewmodel_snapshot(owner.state, owner.bindings, context->command.number);
}
const NativePredictionObserverVtable history_entries{capture_history, finish_history,
                                                     unchanged_tick, empty_rejected};
const NativePredictionObserverVtable quick_switch_entries{capture_quick_switch, finish_quick_switch,
                                                          unchanged_tick, empty_rejected};
const NativePredictionObserverVtable tick_shift_entries{empty_command, empty_command, tick_shift_tick,
                                                     empty_rejected};
const NativePredictionObserverVtable entity_entries{capture_entity_commands, finish_entity_commands,
                                                    unchanged_tick, reject_entity_commands};
const NativePredictionObserverVtable script_entries{capture_script, finish_script, unchanged_tick,
                                                    empty_rejected};
const NativePredictionObserverVtable viewmodel_entries{capture_viewmodel, finish_viewmodel,
                                                       unchanged_tick, reject_viewmodel};

NativePredictionObserverReceiver& source_receiver(void* endpoint) {
    auto* receiver = static_cast<NativePredictionObserverReceiver*>(endpoint);
    if (!receiver ||
        (receiver->vtable != &history_entries && receiver->vtable != &quick_switch_entries &&
         receiver->vtable != &tick_shift_entries && receiver->vtable != &entity_entries &&
         receiver->vtable != &script_entries && receiver->vtable != &viewmodel_entries))
        throw std::logic_error("Native prediction entry has no concrete source receiver binding");
    return *receiver;
}
}
std::span<const ObserverTable> native_prediction_observer_tables() noexcept {
    return tables;
}
std::optional<ObserverOwnerId> identify_prediction_observer(std::uint32_t vtable) noexcept {
    for (const auto& table : tables)
        if (std::uint32_t(table.id) == vtable)
            return table.id;
    return std::nullopt;
}
ScriptPredictionObserver::ScriptPredictionObserver(script::Bridge& source,
                                                   ObserverRegistry& registry)
    : bridge(source), before_event(source.name(events::EventName::pre_predict_command)),
      after_event(source.name(events::EventName::predict_command)),
      command_number_field(source.name(events::EventName::command_number)) {
    registry.append(*this); //4347FF8A; table publication remains4347FFC9
}
ObserverRegistry::ObserverRegistry(engine::PreservedInitialization& initialization,
                                   engine::JobMemory memory)
    : ObserverRegistry(initialization.image_owner(), memory) {}
ObserverRegistry::ObserverRegistry(std::shared_ptr<engine::NativeDataImage> image,
                                   engine::JobMemory memory)
    : image_(image),
      owners_(engine::require_native_data_image(image_).native_cell<engine::OwnerRegistry>(
          0x43475AB0u)),
      memory_(memory) {
    if (!memory_.allocate || !memory_.free || !memory_.process_heap)
        throw std::invalid_argument(
            "Native prediction storage requires the canonical process heap");
}
void ObserverRegistry::append_receiver(NativePredictionObserverReceiver& receiver, void* source) {
    receiver.source = source;
    engine::register_owner(owners_, &receiver, memory_); //exact4341CEE4
}
void ObserverRegistry::append(client::PredictionHistoryOwner& owner) {
    append_receiver(owner.prediction_receiver, &owner);
}
void ObserverRegistry::append(misc::QuickSwitchRuntime& owner) {
    append_receiver(owner.prediction_receiver, &owner);
}
void ObserverRegistry::append(network::TickShiftRuntimeState& owner) {
    append_receiver(owner.prediction_receiver, &owner);
}
void ObserverRegistry::append(client::EntityCommandOwner& owner) {
    append_receiver(owner.prediction_receiver, &owner);
}
void ObserverRegistry::append(ScriptPredictionObserver& owner) {
    append_receiver(owner.receiver, &owner);
}
void ObserverRegistry::append(ViewmodelPredictionObserver& owner) {
    append_receiver(owner.receiver, &owner);
}
void ObserverRegistry::publish(client::PredictionHistoryOwner& owner) const noexcept {
    owner.prediction_receiver.vtable = &history_entries;
}
void ObserverRegistry::publish(misc::QuickSwitchRuntime& owner) const noexcept {
    owner.prediction_receiver.vtable = &quick_switch_entries;
}
void ObserverRegistry::publish(network::TickShiftRuntimeState& owner) const noexcept {
    owner.prediction_receiver.vtable = &tick_shift_entries;
}
void ObserverRegistry::publish(client::EntityCommandOwner& owner) const noexcept {
    owner.prediction_receiver.vtable = &entity_entries;
}
void ObserverRegistry::publish(ScriptPredictionObserver& owner) const noexcept {
    owner.receiver.vtable = &script_entries;
}
void ObserverRegistry::publish(ViewmodelPredictionObserver& owner) const noexcept {
    owner.receiver.vtable = &viewmodel_entries;
}

void ObserverRegistry::capture_before(void* player, const client::Command& command) const {
    //433EB55D..56B captures begin before end for ordinary slot0.
    auto** cursor = owners_.begin;
    auto** const end = owners_.end;
    for (; cursor != end; ++cursor) {
        auto& receiver = source_receiver(*cursor);
        receiver.vtable->capture(&receiver, player, &command);
    }
}
void ObserverRegistry::finish_prediction(void* player, const client::Command& command) const {
    //433EB643/649 captures begin before end and decrements before slot1.
    auto** const begin = owners_.begin;
    auto** cursor = owners_.end;
    while (cursor != begin) {
        auto& receiver = source_receiver(*--cursor);
        receiver.vtable->finish(&receiver, player, &command);
    }
}
void ObserverRegistry::finish_forward(void* player, const client::Command& command) const {
    //4335A283/289 takes a fresh end-before-begin snapshot for this caller.
    auto** const end = owners_.end;
    auto** cursor = owners_.begin;
    for (; cursor != end; ++cursor) {
        auto& receiver = source_receiver(*cursor);
        receiver.vtable->finish(&receiver, player, &command);
    }
}
void ObserverRegistry::rejected(void* player, const PlayerCommandContext& context) const {
    //433EB4F4/4FA: end then begin; pass the context BASE to slot3.
    auto** const end = owners_.end;
    auto** cursor = owners_.begin;
    for (; cursor != end; ++cursor) {
        auto& receiver = source_receiver(*cursor);
        receiver.vtable->rejected(&receiver, player, &context);
    }
}
std::int32_t ObserverRegistry::adjust_tick(std::int32_t number, std::int32_t tick) const {
    //Every proven slot2 caller captures end then begin and chains EAX.
    auto** const end = owners_.end;
    auto** cursor = owners_.begin;
    for (; cursor != end; ++cursor) {
        auto& receiver = source_receiver(*cursor);
        tick = receiver.vtable->tick(&receiver, number, tick);
    }
    return tick;
}
void ObserverRegistry::finalize_storage() const {
    engine::UnloadSystemFunctions memory;
    memory.free_heap = memory_.free;
    memory.process_heap = memory_.process_heap;
    engine::free_native_allocation(owners_.begin, memory); //complete433F515B; no cell stores
}
std::int32_t adjust_observer_tickbases(const ObserverRegistry& registry,
                                       const client::Command& command, std::int32_t tick) {
    return registry.adjust_tick(command.number, tick);
}
} //namespace skeet::prediction

#undef PREDICTION_OBSERVER_FAST
