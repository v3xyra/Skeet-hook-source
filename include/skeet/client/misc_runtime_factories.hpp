#pragma once
#include "skeet/misc/automatic_weapons_owner.hpp"
#include "skeet/misc/clan_tag_owner.hpp"
#include "skeet/misc/grenade_owner.hpp"
#include "skeet/misc/knife_owner.hpp"
#include "skeet/misc/low_fps_runtime.hpp"
#include "skeet/misc/no_fall_damage.hpp"
#include "skeet/misc/quick_switch_owner.hpp"
#include "skeet/misc/rebuy_fix_owner.hpp"
#include "skeet/misc/zeus_owner.hpp"

namespace skeet::client {
class NativeStartupSources;

struct MiscLowFpsFactoryContext {
    rage::PerformanceSettings& settings;
    const rage::GlobalSettings& rage;
    misc::LowFpsBindings engine;
    script::Bridge& scripts;
    const InputBitmap& input;
    const esp::IndicatorLayout& layout;
    const std::uint32_t& life_state_offset;
    misc::LowFpsRuntime*& active; // borrowed source projection, no native global store
};
struct MiscQuickSwitchFactoryContext {
    misc::QuickSwitchConfig& settings;
    misc::QuickSwitchRuntime& runtime;
};
struct MiscGrenadeFactoryContext {
    misc::GrenadePredictionConfig& settings;
    esp::GrenadeEspConfig& visuals;
    misc::GrenadeOwnerBindings engine;
};
struct MiscNoFallDamageFactoryContext {
    misc::NoFallDamageConfig& settings;
    misc::NoFallDamageBindings engine;
};
struct MiscAutomaticWeaponsFactoryContext {
    misc::AutomaticWeaponsConfig& settings;
    misc::AutomaticWeaponsState& state;
    const misc::AutomaticWeaponsBindings& engine;
};
struct MiscKnifeFactoryContext {
    misc::KnifeConfig& settings;
    misc::KnifeCommandBindings engine;
    std::uint32_t& scheduler_flags;
};
struct MiscZeusFactoryContext {
    misc::ZeusConfig& settings;
    misc::ZeusState& state;
    const misc::ZeusBindings& engine;
};
struct MiscClanTagFactoryContext {
    misc::ClanTagConfig& settings;
    misc::ClanTagRuntime& runtime;
    const ScriptCommandEnvironment& environment;
};
struct MiscRebuyFactoryContext {
    misc::RebuyFixOwnerBindings engine;
};

// These contexts borrow the same feature state/settings used by command,
// entity, HUD and menu consumers. Keep every reference and this record alive
// through owner teardown. Binding supplies bodies, not a factory order.
struct MiscRuntimeFactoryBindings {
    ConfigRegistries& configuration;
    engine::OwnerRegistryBinding lifecycle; //43475B78
    engine::OwnerRegistryBinding hud; //43475A9C
    engine::OwnerRegistryBinding updates; //43475988
    engine::OwnerRegistryBinding frames; //43475AD8
    engine::OwnerRegistryBinding messages; //43475B94
    CommandPhaseRegistry& commands;
    CommandEmptyTickRegistry& empty_ticks;
    EntityObserverRegistry& entities;
    network::TickShiftRechargeObserverRegistry& recharge;
    prediction::ObserverRegistry& prediction;
    MiscLowFpsFactoryContext low_fps;
    MiscQuickSwitchFactoryContext quick_switch;
    MiscGrenadeFactoryContext grenades;
    MiscNoFallDamageFactoryContext no_fall_damage;
    MiscAutomaticWeaponsFactoryContext automatic_weapons;
    MiscKnifeFactoryContext knife;
    MiscZeusFactoryContext zeus;
    MiscClanTagFactoryContext clan_tag;
    MiscRebuyFactoryContext rebuy;
};

//434903AD,43481DC0,43481EDA,4348401C,4348E51F,43483FF8,43490389,
//4348DA01,4347E2E3. Native INT1 allocation-failure transfers remain unrecovered;
// the eight affected source factories use an explicit bad_alloc failure policy.
// Grenade preserves its proved allocation-failure null publication instead.
void bind_misc_runtime_factories(NativeStartupSources&, MiscRuntimeFactoryBindings&);
} //namespace skeet::client
