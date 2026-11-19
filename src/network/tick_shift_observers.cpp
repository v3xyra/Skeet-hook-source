#include "skeet/network/tick_shift_observers.hpp"
#include "skeet/client/entity_command_owner.hpp"
#include "skeet/client/prediction_history_owner.hpp"
#include "skeet/misc/viewmodel_lag.hpp"
#include "skeet/rage/shot_feedback.hpp"
#include <bit>
#include "skeet/engine/unload.hpp"
#include <stdexcept>

#if defined(_M_IX86) || defined(__i386__)
#define TICK_SHIFT_RECHARGE_FAST __fastcall
#define TICK_SHIFT_RECHARGE_MEMBER __thiscall
#else
#define TICK_SHIFT_RECHARGE_FAST
#define TICK_SHIFT_RECHARGE_MEMBER
#endif

namespace skeet::network {
namespace {
void TICK_SHIFT_RECHARGE_FAST empty_generated(NativeTickShiftRechargeReceiver*, void*, std::int32_t,
                                           std::int32_t) {} //4334A269
void TICK_SHIFT_RECHARGE_FAST empty_recharge(NativeTickShiftRechargeReceiver*, void*, std::int32_t) {
} //433C95B7
void TICK_SHIFT_RECHARGE_MEMBER empty_cloned(NativeTickShiftRechargeReceiver*, void*, client::Command*) {
} //4334A269
void TICK_SHIFT_RECHARGE_FAST recharge_prediction(NativeTickShiftRechargeReceiver* receiver, void*,
                                               std::int32_t number) {
    client::clone_previous_prediction_snapshot(
        *static_cast<client::PredictionHistoryOwner*>(receiver->source), number);
}
void TICK_SHIFT_RECHARGE_FAST recharge_entity_commands(NativeTickShiftRechargeReceiver* receiver, void*,
                                                    std::int32_t number) {
    client::observe_entity_command_recharge(
        *static_cast<client::EntityCommandOwner*>(receiver->source), number);
}
void TICK_SHIFT_RECHARGE_FAST recharge_viewmodel(NativeTickShiftRechargeReceiver* receiver, void*,
                                              std::int32_t) {
    auto& owner = *static_cast<TickShiftViewmodelRecharge*>(receiver->source);
    misc::advance_viewmodel_prediction(owner.state, owner.bindings);
}
void TICK_SHIFT_RECHARGE_FAST generated_feedback(NativeTickShiftRechargeReceiver* receiver, void*,
                                              std::int32_t final_command, std::int32_t) {
    auto& owner = *static_cast<TickShiftShotFeedbackObserver*>(receiver->source);
    // Complete433C86CE: word1 is retained; only mutable word3 changes.
    for (auto& record : owner.state.pending) {
        const auto command = std::bit_cast<std::int32_t>(record.words[1]);
        if (owner.last_outgoing < command && command < final_command)
            record.words[3] = std::uint32_t(final_command);
    }
}
void TICK_SHIFT_RECHARGE_FAST generated_viewmodel(NativeTickShiftRechargeReceiver* receiver, void*,
                                               std::int32_t final_command, std::int32_t count) {
    auto& owner = *static_cast<TickShiftViewmodelRecharge*>(receiver->source);
    misc::record_viewmodel_command_range(owner.state, owner.bindings, final_command, count);
}
const NativeTickShiftRechargeVtable prediction_entries{empty_generated, recharge_prediction,
                                                     empty_cloned};
const NativeTickShiftRechargeVtable noop_entries{empty_generated, empty_recharge, empty_cloned};
const NativeTickShiftRechargeVtable feedback_entries{generated_feedback, empty_recharge,
                                                   empty_cloned};
const NativeTickShiftRechargeVtable entity_entries{empty_generated, recharge_entity_commands,
                                                 empty_cloned};
const NativeTickShiftRechargeVtable viewmodel_entries{generated_viewmodel, recharge_viewmodel,
                                                    empty_cloned};

NativeTickShiftRechargeReceiver& source_receiver(void* endpoint) {
    auto* receiver = static_cast<NativeTickShiftRechargeReceiver*>(endpoint);
    if (!receiver ||
        (receiver->vtable != &prediction_entries && receiver->vtable != &noop_entries &&
         receiver->vtable != &feedback_entries && receiver->vtable != &entity_entries &&
         receiver->vtable != &viewmodel_entries))
        throw std::logic_error("Native recharge entry has no concrete source receiver binding");
    return *receiver;
}
}
TickShiftRechargeObserverRegistry::TickShiftRechargeObserverRegistry(
    engine::PreservedInitialization& initialization, engine::JobMemory memory)
    : TickShiftRechargeObserverRegistry(initialization.image_owner(), memory) {}
TickShiftRechargeObserverRegistry::TickShiftRechargeObserverRegistry(
    std::shared_ptr<engine::NativeDataImage> image, engine::JobMemory memory)
    : image_(image),
      owners_(engine::require_native_data_image(image_).native_cell<engine::OwnerRegistry>(
          0x43475784u)),
      memory_(memory) {
    if (!memory_.allocate || !memory_.free || !memory_.process_heap)
        throw std::invalid_argument("Native recharge storage requires the canonical process heap");
}
void TickShiftRechargeObserverRegistry::append_receiver(NativeTickShiftRechargeReceiver& receiver,
                                                      void* source) {
    receiver.source = source;
    engine::register_owner(owners_, &receiver, memory_);
}
void TickShiftRechargeObserverRegistry::append(client::PredictionHistoryOwner& owner) {
    append_receiver(owner.recharge_receiver, &owner);
}
void TickShiftRechargeObserverRegistry::append(client::EntityCommandOwner& owner) {
    append_receiver(owner.recharge_receiver, &owner);
}
void TickShiftRechargeObserverRegistry::append(TickShiftShotFeedbackObserver& owner) {
    append_receiver(owner.receiver, &owner);
}
void TickShiftRechargeObserverRegistry::append(TickShiftViewmodelRecharge& owner) {
    append_receiver(owner.receiver, &owner);
}
void TickShiftRechargeObserverRegistry::append_noop(NativeTickShiftRechargeReceiver& receiver,
                                                  void* source) {
    append_receiver(receiver, source);
}
void TickShiftRechargeObserverRegistry::publish(
    client::PredictionHistoryOwner& owner) const noexcept {
    owner.recharge_receiver.vtable = &prediction_entries;
}
void TickShiftRechargeObserverRegistry::publish(client::EntityCommandOwner& owner) const noexcept {
    owner.recharge_receiver.vtable = &entity_entries;
}
void TickShiftRechargeObserverRegistry::publish(TickShiftShotFeedbackObserver& owner) const noexcept {
    owner.receiver.vtable = &feedback_entries;
}
void TickShiftRechargeObserverRegistry::publish(TickShiftViewmodelRecharge& owner) const noexcept {
    owner.receiver.vtable = &viewmodel_entries;
}
void TickShiftRechargeObserverRegistry::publish_noop(
    NativeTickShiftRechargeReceiver& receiver) const noexcept {
    receiver.vtable = &noop_entries;
}

void TickShiftRechargeObserverRegistry::notify_recharge(void* player, std::int32_t number) const {
    //433AC27B/281: capture end, then begin, once before any slot1 call.
    auto** const end = owners_.end;
    auto** cursor = owners_.begin;
    for (; cursor != end; ++cursor) {
        auto& receiver = source_receiver(*cursor);
        receiver.vtable->recharge(&receiver, player, number);
    }
}
void TickShiftRechargeObserverRegistry::notify_generated(void* player, std::int32_t final_command,
                                                       std::int32_t count) const {
    //4335B693/6A0: end and begin are captured before the first slot0 call.
    auto** const end = owners_.end;
    auto** cursor = owners_.begin;
    for (; cursor != end; ++cursor) {
        auto& receiver = source_receiver(*cursor);
        receiver.vtable->generated(&receiver, player, final_command, count);
    }
}
void TickShiftRechargeObserverRegistry::notify_cloned(void* player, client::Command& command) const {
    //433674CF/D5 retains actual slot2 traversal, including genuine RET8 entries.
    auto** const end = owners_.end;
    auto** cursor = owners_.begin;
    for (; cursor != end; ++cursor) {
        auto& receiver = source_receiver(*cursor);
        receiver.vtable->cloned(&receiver, player, &command);
    }
}
void TickShiftRechargeObserverRegistry::finalize_storage() const {
    engine::UnloadSystemFunctions memory;
    memory.free_heap = memory_.free;
    memory.process_heap = memory_.process_heap;
    engine::free_native_allocation(owners_.begin, memory);
}
void notify_tick_shift_recharge_observers(const TickShiftRechargeObserverRegistry& registry,
                                       void* player, std::int32_t number) {
    registry.notify_recharge(player, number);
}
void notify_tick_shift_generated_observers(const TickShiftRechargeObserverRegistry& registry,
                                        void* player, std::int32_t final_command,
                                        std::int32_t count) {
    registry.notify_generated(player, final_command, count);
}
} //namespace skeet::network

#undef TICK_SHIFT_RECHARGE_FAST
#undef TICK_SHIFT_RECHARGE_MEMBER
