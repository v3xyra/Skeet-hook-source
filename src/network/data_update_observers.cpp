#include "skeet/network/data_update_observers.hpp"
#include "skeet/network/tick_shift_acknowledgement.hpp"
#include "skeet/client/entity_command_owner.hpp"
#include "skeet/client/prediction_history_owner.hpp"
#include "skeet/skins/native_agents.hpp"
#include "skeet/engine/unload.hpp"
#include <stdexcept>

#if defined(_M_IX86) || defined(__i386__)
#define SKEET_DATA_UPDATE_FASTCALL __fastcall
#else
#define SKEET_DATA_UPDATE_FASTCALL
#endif

namespace skeet::network {
namespace {
constexpr std::array<DataUpdateObserverTable, 4> tables{{
    {DataUpdateObserverId::prediction_history, {{0x433C95B7U, 0x433F4628U}}},
    {DataUpdateObserverId::agents, {{0x433C95B7U, 0x433466F7U}}},
    {DataUpdateObserverId::tick_shift, {{0x433CCC20U, 0x433EEA67U}}},
    {DataUpdateObserverId::entity_commands, {{0x433D67DFU, 0x43403681U}}},
}};

// Native433C95B7 is the shared, actual RET4 capture entry for these two owners.
void SKEET_DATA_UPDATE_FASTCALL empty_capture(NativeDataUpdateReceiver*, void*, std::int32_t) {}
void SKEET_DATA_UPDATE_FASTCALL finish_prediction(NativeDataUpdateReceiver* receiver, void* player,
                                                  std::int32_t update) {
    client::finish_prediction_data_update(
        *static_cast<client::PredictionHistoryOwner*>(receiver->source), player, update);
}
void SKEET_DATA_UPDATE_FASTCALL finish_agents(NativeDataUpdateReceiver* receiver, void* player,
                                              std::int32_t update) {
    static_cast<skins::NativeAgentRuntime*>(receiver->source)->process(player, update);
}
void SKEET_DATA_UPDATE_FASTCALL capture_tick_shift(NativeDataUpdateReceiver* receiver, void* player,
                                                std::int32_t) {
    capture_tick_shift_acknowledgement(*static_cast<TickShiftAcknowledgementOwner*>(receiver->source),
                                    player);
}
void SKEET_DATA_UPDATE_FASTCALL finish_tick_shift(NativeDataUpdateReceiver* receiver, void* player,
                                               std::int32_t) {
    finish_tick_shift_acknowledgement(*static_cast<TickShiftAcknowledgementOwner*>(receiver->source),
                                   player);
}
void SKEET_DATA_UPDATE_FASTCALL capture_entity_commands(NativeDataUpdateReceiver* receiver,
                                                        void* player, std::int32_t update) {
    client::process_entity_data_update_capture(
        *static_cast<client::EntityCommandOwner*>(receiver->source), player, update);
}
void SKEET_DATA_UPDATE_FASTCALL finish_entity_commands(NativeDataUpdateReceiver* receiver,
                                                       void* player, std::int32_t update) {
    client::process_entity_data_update_finish(
        *static_cast<client::EntityCommandOwner*>(receiver->source), player, update);
}
const NativeDataUpdateVtable prediction_entries{empty_capture, finish_prediction};
const NativeDataUpdateVtable agent_entries{empty_capture, finish_agents};
const NativeDataUpdateVtable tick_shift_entries{capture_tick_shift, finish_tick_shift};
const NativeDataUpdateVtable entity_entries{capture_entity_commands, finish_entity_commands};

NativeDataUpdateReceiver& source_receiver(void* object) {
    auto* const receiver = static_cast<NativeDataUpdateReceiver*>(object);
    if (!receiver ||
        (receiver->vtable != &prediction_entries && receiver->vtable != &agent_entries &&
         receiver->vtable != &tick_shift_entries && receiver->vtable != &entity_entries))
        throw std::logic_error("Native data-update entry has no concrete source receiver binding");
    // Captured native vtable addresses are identifiers only. An unbridged
    // image receiver must not execute captured code as a dispatch substitute.
    return *receiver;
}
}
std::span<const DataUpdateObserverTable> native_data_update_observer_tables() noexcept {
    return tables;
}
std::optional<DataUpdateObserverId> identify_data_update_observer(std::uint32_t address) noexcept {
    for (const auto& table : tables)
        if (std::uint32_t(table.id) == address)
            return table.id;
    return std::nullopt;
}
DataUpdateObserverRegistry::DataUpdateObserverRegistry(
    engine::PreservedInitialization& initialization, engine::JobMemory memory)
    : DataUpdateObserverRegistry(initialization.image_owner(), memory) {}
DataUpdateObserverRegistry::DataUpdateObserverRegistry(
    std::shared_ptr<engine::NativeDataImage> image, engine::JobMemory memory)
    : image_(image),
      owners_(engine::require_native_data_image(image_).native_cell<engine::OwnerRegistry>(
          0x43475760u)),
      memory_(memory) {
    if (!memory_.allocate || !memory_.free || !memory_.process_heap)
        throw std::invalid_argument(
            "Native data-update storage requires the canonical process heap");
}

void DataUpdateObserverRegistry::append_receiver(NativeDataUpdateReceiver& receiver, void* source) {
    receiver.source = source;
    // Exact4337EFD9: register_owner retains its -4 sentinel handling and the
    //433F1665/4334DC99 rereads after allocation. Final secondary tables are
    //published separately at their actual later constructor write sites.
    engine::register_owner(owners_, &receiver, memory_);
}
void DataUpdateObserverRegistry::append(client::PredictionHistoryOwner& owner) {
    append_receiver(owner.data_update_receiver, &owner);
}
void DataUpdateObserverRegistry::append(skins::NativeAgentRuntime& owner) {
    append_receiver(owner.data_update_receiver_, &owner);
}
void DataUpdateObserverRegistry::append(TickShiftAcknowledgementOwner& owner) {
    append_receiver(owner.data_update_receiver, &owner);
}
void DataUpdateObserverRegistry::append(client::EntityCommandOwner& owner) {
    append_receiver(owner.data_update_receiver, &owner);
}
void DataUpdateObserverRegistry::publish(client::PredictionHistoryOwner& owner) const noexcept {
    owner.data_update_receiver.vtable = &prediction_entries;
}
void DataUpdateObserverRegistry::publish(skins::NativeAgentRuntime& owner) const noexcept {
    owner.data_update_receiver_.vtable = &agent_entries;
}
void DataUpdateObserverRegistry::publish(TickShiftAcknowledgementOwner& owner) const noexcept {
    owner.data_update_receiver.vtable = &tick_shift_entries;
}
void DataUpdateObserverRegistry::publish(client::EntityCommandOwner& owner) const noexcept {
    owner.data_update_receiver.vtable = &entity_entries;
}

void DataUpdateObserverRegistry::capture_forward(void* player, std::int32_t update) const {
    //433B7680 reads begin, then433B7687 reads end. Do not reread either cell
    //after a receiver call; insertion during dispatch is outside this range.
    auto** cursor = owners_.begin;
    auto** const end = owners_.end;
    for (; cursor != end; ++cursor) {
        auto& receiver = source_receiver(*cursor);
        receiver.vtable->capture(&receiver, player, update);
    }
}
void DataUpdateObserverRegistry::finish_reverse(void* player, std::int32_t update) const {
    //433B711D/433B7123 capture begin then end; decrement precedes slot1.
    auto** const begin = owners_.begin;
    auto** cursor = owners_.end;
    while (cursor != begin) {
        auto& receiver = source_receiver(*--cursor);
        receiver.vtable->finish(&receiver, player, update);
    }
}
void DataUpdateObserverRegistry::finalize_storage() const {
    engine::UnloadSystemFunctions functions;
    functions.free_heap = memory_.free;
    functions.process_heap = memory_.process_heap;
    // Complete433F4A1D: load/null-test actual43475760, free its aligned native
    //allocation, leave begin/end/limit untouched. No automatic second release.
    engine::free_native_allocation(owners_.begin, functions);
}
} //namespace skeet::network

#undef SKEET_DATA_UPDATE_FASTCALL
