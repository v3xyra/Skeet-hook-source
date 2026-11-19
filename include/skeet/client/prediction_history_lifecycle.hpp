#pragma once
#include "prediction_history_owner.hpp"
#include "command_empty_tick.hpp"
#include "command_phase_registry.hpp"
#include "entity_observer_registry.hpp"
#include "skeet/config.hpp"
#include "skeet/engine/owner_registry.hpp"
#include "skeet/network/data_update_observers.hpp"
#include "skeet/network/tick_shift_observers.hpp"
#include "skeet/prediction/observer_registry.hpp"

namespace skeet::client {
class NativeStartupSources;
struct PredictionHistoryRegistries {
    engine::OwnerRegistryBinding primary; //43475B78
    CommandPhaseRegistry& commands; //434757A0
    CommandEmptyTickRegistry& empty_ticks; //actual43475978 buffer
    prediction::ObserverRegistry& prediction; //43475AB0
    EntityObserverRegistry& entities; //43475BA8
    network::DataUpdateObserverRegistry& data_updates; //43475760
    network::TickShiftRechargeObserverRegistry& recharge; //actual43475784 buffer
};
struct PredictionHistoryLifecycleBindings {
    // The same canonical state/views consumed by commands, prediction, RAGE
    // and DataUpdate. Construct storage with deferred_prediction_history_storage;
    // owner.shared and redirect references must refer to this storage.
    PredictionHistoryStorage& storage;
    PredictionHistoryOwner& owner;
    const std::int32_t* const& acknowledged_command; //live43475BA4 cell; recorded MOV at4347D094
    void* const& animation_allocator; //live43467204, genuine Free virtual5
    PredictionHistoryOwner*& active; //must alias DataUpdate image's actual434759B4 cell
};

std::size_t prediction_history_owner_size() noexcept;
// Whole4347D010. Registers primary/command/empty/prediction/entity/DataUpdate/
// recharge in that order. DataUpdate table is published only at4347D064,
// recharge at06D; primary follows at079, empty tick086, prediction08D.
// Both native buffers and22owned strings are
// initialized afterward, without a semantic-buffer substitute on failure.
void* construct_prediction_history_owner(void* storage, const PredictionHistoryRegistries&,
                                         ConfigRegistries&, PredictionHistoryLifecycleBindings);
// Whole4349081E. A failed outer allocation publishes null; success publishes
// the canonical owner only after construction. No cross-owner order is chosen.
// The publication at43490837 is the explicit mem_int3.h instruction record;
// its original five bytes are traps, not an ordinary raw MOV instruction.
void* create_prediction_history_owner(const PredictionHistoryRegistries&, ConfigRegistries&,
                                      PredictionHistoryLifecycleBindings);
// Whole433B7312 ->433AE3CF ->433F9238. Clears active before the null test,
// destroys strings in reverse, releases records then snapshot vectors/cookie,
// and finally releases the primary allocation. Registry buffers remain owned
// by their actual later finalizers; no receiver is removed here.
void destroy_prediction_history_owner(void* storage, PredictionHistoryOwner*& active);
void bind_prediction_history_owner_settings(void* storage); //4348015A, leg movement
void reset_prediction_history_owner(void* storage); //433F2757, exact sparse reset
void refresh_prediction_history_owner(void* storage); //433CF2F5 -> current primary slot5

struct PredictionHistoryInitialization {
    const PredictionHistoryRegistries& registries;
    ConfigRegistries& configuration;
    PredictionHistoryLifecycleBindings bindings;
};
// Binds only the actual4349081E initializer identity. The existing startup
// graph supplies invocation order; registering this source does not invoke it.
void bind_prediction_history_initializer(NativeStartupSources&, PredictionHistoryInitialization&);
} // namespace skeet::client
