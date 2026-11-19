#pragma once
#include "entity_command_owner.hpp"
#include "entity_observer_registry.hpp"
#include "command_phase_registry.hpp"
#include "skeet/engine/owner_registry.hpp"
#include "skeet/engine/native_data_image.hpp"
#include "skeet/network/data_update_observers.hpp"
#include "skeet/network/tick_shift_observers.hpp"
#include "skeet/prediction/observer_registry.hpp"

namespace skeet::client {
class NativeStartupSources;

struct EntityCommandOwnerRegistries {
    engine::OwnerRegistryBinding lifecycle; //433676A0
    CommandPhaseRegistry& commands; //43345EBC
    prediction::ObserverRegistry& prediction; //4341CEE4
    network::DataUpdateObserverRegistry& data_updates; //4337EFD9
    EntityObserverRegistry& entities; //4333BF32
    engine::OwnerRegistryBinding map_entities; //433817AA
    network::TickShiftRechargeObserverRegistry& recharge; //4334E132
};
struct EntityCommandFactoryBindings {
    EntityCommandOwnerRegistries registries;
    EntityCommandOwner& command;
    // The mutable object referenced by command.bindings, not another copy.
    EntityCommandBindings& command_bindings;
    std::shared_ptr<engine::NativeDataImage> data;
    void* const& engine_client; //4346A514
    void* const& prediction_interface; //43469264
    const std::uint8_t& shot_recording_enabled; //43467224
    double (*ceiling)(double); //actual recorded CRT ceil77512A30
};

std::size_t entity_command_owner_size() noexcept;
// Whole constructor4347F928. The same caller-supplied command/state remains
// the receiver of all six registered secondary interfaces; neither is copied.
void construct_entity_command_owner(void* storage, const EntityCommandFactoryBindings&);
// Whole factory43486705. Publishes the actual43475898 source-owner slot only
// after construction; allocation failure publishes null, as its proven branch.
void* create_entity_command_owner(const EntityCommandFactoryBindings&);
void destroy_entity_command_owner(void* storage, const engine::NativeDataImage&);
void reset_entity_command_owner(void* storage); //43388375
void refresh_entity_command_owner(void* storage); //43384729
// Bind the actual factory identity without selecting another factory's order.
void bind_native_entity_command_factory(NativeStartupSources&, EntityCommandFactoryBindings&);
} // namespace skeet::client
