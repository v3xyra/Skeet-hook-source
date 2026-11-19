#pragma once
#include "viewmodel_hooks.hpp"
#include "skeet/client/command_phase_registry.hpp"
#include "skeet/client/entity_observer_registry.hpp"
#include "skeet/engine/frame_update.hpp"
#include "skeet/network/tick_shift_observers.hpp"
#include "skeet/prediction/observer_registry.hpp"
#include <vector>

namespace skeet::client {
class NativeStartupSources;
}

namespace skeet::misc {
inline constexpr std::size_t viewmodel_lag_owner_allocation = 0x17B8;
struct ViewmodelLagOwnerRegistries {
    engine::OwnerRegistryBinding primary; //43475B78
    prediction::ObserverRegistry& prediction; //43475AB0
    client::EntityObserverRegistry& entities; //43475BA8
    client::CommandPhaseRegistry& commands; //434757A0
    network::TickShiftRechargeObserverRegistry& recharge; //43475784
    engine::OwnerRegistryBinding frames; //43475AD8
};

// Complete43486C3D. Borrow the one state/hooks pair, with state.snapshots bound
// to canonical skins::SequenceRuntime.history. Both must outlive these six
// registered receivers and the installed viewmodel hooks. This constructor
// performs the native sparse initialization after registering every interface;
// it neither prepares hook metadata nor installs a receiver vtable.
// Native43387448 uses heap flag8 for fresh backing. The external canonical
// state/history backing must supply that allocation policy; this constructor
// preserves their otherwise-unassigned words instead of adding a full reset.
std::size_t viewmodel_lag_owner_size() noexcept;
ViewmodelLagOwner* construct_viewmodel_lag_owner(void* native_aligned_storage,
                                                 const ViewmodelLagOwnerRegistries&,
                                                 ViewmodelHooksRuntime&);

// Complete4347E13E..4347E15C, including recorded MOV[43475860],EAX at E157.
// Allocate exactly0x17B8, construct on success, then publish the full primary
// owner or null into hooks.shared_owner(). The semantic state is never stored
// in that owner slot. The returned pointer borrows primary-registry ownership.
ViewmodelLagOwner* create_viewmodel_lag_owner(const ViewmodelLagOwnerRegistries&,
                                              ViewmodelHooksRuntime&);

// Explicit source projection of the native full owner to its canonical state.
// Nullable hook callers preserve their original null-owner gate. Both state
// and its snapshot array remain the same borrowed objects used by every view.
ViewmodelLagState* viewmodel_lag_owner_state(ViewmodelLagOwner*) noexcept;

// Complete433A102F ->433837D0: clear the canonical43475860 source slot, then
// release only the native-header primary allocation. Registered pointer ranges
// retain their actual teardown ownership; borrowed history is not destroyed.
void destroy_viewmodel_lag_owner(ViewmodelLagOwner*, ViewmodelLagOwner*& active);
void reset_viewmodel_lag_owner(ViewmodelLagOwner*); //43341C91
void refresh_viewmodel_lag_owner(ViewmodelLagOwner*); //433CF2F5 ->current primary slot5

// The caller retains this context until all initialization calls finish.
// Binding supplies only the proven factory identity; its invocation order is
// supplied by NativeStartupSources' existing actual scheduler.
struct ViewmodelLagFactoryContext {
    const ViewmodelLagOwnerRegistries& registries;
    ViewmodelHooksRuntime& hooks;
};
void bind_viewmodel_lag_factory(client::NativeStartupSources&, ViewmodelLagFactoryContext&);
} //namespace skeet::misc
