#pragma once
#include "tick_shift_acknowledgement.hpp"
#include "tick_shift_command_owner.hpp"
#include "tick_shift_message.hpp"
#include "data_update_observers.hpp"
#include "skeet/client/command_phase_registry.hpp"
#include "skeet/hud/composite_handlers.hpp"
#include "skeet/prediction/observer_registry.hpp"
#include "skeet/rage/settings.hpp"

namespace skeet::network {
struct TickShiftOwnerRegistries {
    engine::OwnerRegistryBinding primary; //43475B78
    engine::OwnerRegistryBinding hud; //43475A9C
    engine::OwnerRegistryBinding messages; //43475B94
    client::CommandPhaseRegistry& commands; //434757A0
    DataUpdateObserverRegistry& data_updates; //43475760
    prediction::ObserverRegistry& prediction; //43475AB0
};
struct TickShiftOwnerHudBindings {
    script::Bridge& scripts;
    float& shared_timer; //43475890
    const IntegerConfig& indicators;
    const hud::TickShiftIndicatorBindings& interfaces;
    const float& frame_time;
    const esp::IndicatorLayout& layout;
};
struct TickShiftOwnerBindings {
    TickShiftCommandOwner& command;
    const TickShiftAcknowledgementBindings& acknowledgement;
    const TickShiftMessageBindings& messages;
    const TickShiftOwnerHudBindings& hud;
};
std::size_t tick_shift_owner_size() noexcept;
// Complete4347DEEB: primary -> HUD -> SendNetMsg -> command -> data-update
// -> prediction registration, table publication, settings and sparse state.
// All interfaces borrow command.runtime/phases; no second tick-shift state exists.
// command.controls.settings supplies fresh descriptors, registered exactly once.
// Storage is a native aligned allocation with the43387448 header.
void construct_tick_shift_owner(void* storage, const TickShiftOwnerRegistries&, ConfigRegistries&,
                             TickShiftOwnerBindings);
inline constexpr std::size_t native_tick_shift_owner_allocation_size = 0x908;
//4347FF2F..4347FF39 requests exactly908 bytes. The nullable result is exposed
//separately: the native failure branch reaches unresolved INT1 at4347FF44.
//No constructor, state reset or substitute continuation is invoked on failure.
void* allocate_tick_shift_owner_storage(engine::JobMemory);
//The proven success branch4347FF3D..4347FF43 on valid newly allocated storage.
//Applies the native zeroed backing to the one borrowed state, then invokes the
//sparse constructor. This is not the unresolved full factory failure path.
void construct_allocated_tick_shift_owner(void* storage, const TickShiftOwnerRegistries&,
                                       ConfigRegistries&, TickShiftOwnerBindings);
//434707D0 slot0 ->433B1A7A ->433837D0. No unregister/reset is performed.
void destroy_tick_shift_owner(void* storage);
//434707D0 slot7 (433CF2F5) dispatches the current primary slot5.
void refresh_tick_shift_owner(void* storage);
} //namespace skeet::network
