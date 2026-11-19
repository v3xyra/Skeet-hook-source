#pragma once
#include "grenade_command.hpp"
#include "skeet/client/command_empty_tick.hpp"
#include "skeet/client/entity_observer_registry.hpp"
#include "skeet/effects/projectile_ribbons.hpp"
#include "skeet/esp/grenade_live_bindings.hpp"
#include "skeet/hud/composite_runtime.hpp"
#include "skeet/network/update_observers.hpp"
#include "skeet/network/tick_shift_observers.hpp"

namespace skeet::misc {
struct GrenadeOwnerRegistries {
    engine::OwnerRegistryBinding lifecycle; //43475B78
    client::CommandPhaseRegistry& commands; //434757A0
    client::CommandEmptyTickRegistry& empty_ticks; //43475978
    engine::OwnerRegistryBinding hud; //43475A9C
    client::EntityObserverRegistry& entities; //43475BA8
    engine::OwnerRegistryBinding updates; //43475988
    network::TickShiftRechargeObserverRegistry& recharge; //43475784
};
struct GrenadeOwnerBindings {
    esp::GrenadeLiveBindings live;
    void* const& input; //4346A894
    const std::int32_t& command_tick; //*43475A60+4
    const std::uint8_t& release_choke_enabled; //43467560
    const std::uint8_t& observer_suppressed; //434684E0
    client::CommandAngleBindings view_angles;
    MovementReleaseBinding release_movement; //canonical434759B0 view
    MovementFeedbackBinding movement_feedback; //same owner's+1AC view
    const effects::ProjectileRibbonBindings& ribbons;
    const std::uint8_t& ribbons_suppressed; //43468E58
    const std::uint32_t& ribbons_color; //43468F60
    void*& active; //canonical43475AAC
};

// Whole4348675E. All eight owned descriptors must still be unregistered;
// entities/entity_color belong to their separate static ESP initializers.
// Seven endpoint registrations retain native order and precede table setup.
std::size_t grenade_owner_size() noexcept;
void construct_grenade_owner(void* storage, const GrenadeOwnerRegistries&, ConfigRegistries&,
                             GrenadePredictionConfig&, esp::GrenadeEspConfig&,
                             GrenadeOwnerBindings);
void* create_grenade_owner(const GrenadeOwnerRegistries&, ConfigRegistries&,
                           GrenadePredictionConfig&, esp::GrenadeEspConfig&, GrenadeOwnerBindings);
//433E4337 clears the canonical slot before its null check, then4340BC4A and
//433837D0 release owned data and the original aligned allocation header.
void destroy_grenade_owner(void* storage, void*& active);
GrenadeCommandRuntime& grenade_owner_command(void* storage) noexcept;
esp::GrenadeLiveScene& grenade_owner_scene(void* storage) noexcept;
// Concrete433B888D consumer uses this same vector, never a native-offset cast
// or a copied warning array. The native caller guarantees a present owner.
std::span<const esp::GrenadeWarningRecord>
grenade_owner_warning_records(const void* storage) noexcept;
} // namespace skeet::misc
