#pragma once
#include "skeet/client/command_phase_registry.hpp"
#include "skeet/engine/owner_registry.hpp"
#include "skeet/misc/config.hpp"
#include "skeet/misc/quick_switch.hpp"
#include "skeet/network/tick_shift_observers.hpp"
#include "skeet/prediction/observer_registry.hpp"

namespace skeet::misc {
class QuickSwitchLifecycleOwner;
std::size_t quick_switch_owner_size() noexcept;

// Complete4347DC0A. The supplied native aligned allocation is released by
// primary slot0. config is the one STILL-UNREGISTERED canonical descriptor;
// do not also initialize_quick_switch_config for this same value. The borrowed
// runtime/configuration and all three observer ranges outlive this owner.
// Native order: primary -> command -> recharge -> prediction -> descriptor,
// then reset+30/+34 only. Captured command/throw/pin state is preserved.
QuickSwitchLifecycleOwner& construct_quick_switch(
    void* storage, engine::OwnerRegistryBinding lifecycle, client::CommandPhaseRegistry& commands,
    network::TickShiftRechargeObserverRegistry& recharge, prediction::ObserverRegistry&,
    ConfigRegistries&, QuickSwitchConfig&, QuickSwitchRuntime&);
} //namespace skeet::misc
