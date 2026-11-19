#pragma once
#include "skeet/anti_aim/owner_lifecycle.hpp"
#include "skeet/effects/owner_lifecycle.hpp"
#include "skeet/engine/native_owner_registry_storage.hpp"
#include "skeet/network/tick_shift_owner.hpp"
#include "skeet/network/packet_owner.hpp"
#include "skeet/script/owner.hpp"

namespace skeet::client {
class NativeStartupSources;

struct PacketRuntimeFactoryBindings {
    engine::OwnerRegistryBinding lifecycle;
    CommandPhaseRegistry& commands;
    anti_aim::FakeLagSettings& settings;
    network::PacketCommandOwner& command;
};
struct TickShiftRuntimeFactoryBindings {
    network::TickShiftOwnerRegistries registries;
    network::TickShiftOwnerBindings owner;
};
struct AntiAimRuntimeFactoryBindings {
    anti_aim::OwnerRegistries registries;
    anti_aim::OwnerBindings owner;
};
struct EffectsRuntimeFactoryBindings {
    effects::EffectsOwnerRegistries registries;
    effects::EffectsOwnerBindings owner;
};
struct ScriptRuntimeFactoryBindings {
    script::ScriptRuntime& runtime;
    script::ScriptOwnerBindings owner;
    script::ScriptModulesBindings& modules;
};

// All views borrow their existing configuration, command state and registries.
// Keep this context and its borrowed objects at stable addresses through startup
// and through the final registered callbacks. Binding creates no owner.
struct CombatRuntimeFactoryBindings {
    std::shared_ptr<engine::NativeDataImage> data;
    engine::NativeOwnerRegistryStorage& registries;
    ConfigRegistries& configuration;
    PacketRuntimeFactoryBindings packet;
    TickShiftRuntimeFactoryBindings tick_shift;
    AntiAimRuntimeFactoryBindings anti_aim;
    EffectsRuntimeFactoryBindings effects;
    ScriptRuntimeFactoryBindings scripting;
};

inline constexpr std::uint32_t packet_runtime_factory = 0x4347DA84u;
inline constexpr std::uint32_t tick_shift_runtime_factory = 0x4347FF2Fu;
inline constexpr std::uint32_t anti_aim_runtime_factory = 0x43485CCCu;
inline constexpr std::uint32_t effects_runtime_factory = 0x43484F93u;
inline constexpr std::uint32_t script_runtime_factory = 0x4348E608u;

// These are source allocations: retain the native minimum and aligned heap
// header while reserving room for additional typed source binding metadata.
// Packet and tick-shift allocation failure reaches unresolved native INT1/junk; the
// source failure policy throws bad_alloc before construction or registration.
// AA/effects/script preserve their actual null singleton publication branches.
void* create_packet_runtime_owner(CombatRuntimeFactoryBindings&);
void* create_packet_runtime_owner(PacketRuntimeFactoryBindings&, ConfigRegistries&);
void* create_tick_shift_runtime_owner(CombatRuntimeFactoryBindings&);
void* create_anti_aim_runtime_owner(CombatRuntimeFactoryBindings&);
void* create_anti_aim_runtime_owner(AntiAimRuntimeFactoryBindings&, ConfigRegistries&);
void* create_effects_runtime_owner(CombatRuntimeFactoryBindings&);
void* create_effects_runtime_owner(EffectsRuntimeFactoryBindings&, ConfigRegistries&);
script::ScriptOwner* create_script_runtime_owner(CombatRuntimeFactoryBindings&);
// Same4348E608 factory, independently usable once its concrete Lua/module
// dependencies exist. No other combat owner is constructed by this overload.
script::ScriptOwner* create_script_runtime_owner(ScriptRuntimeFactoryBindings&);

// Validates the common image-backed cells and the canonical shared state views,
// then binds these five real factory identities to the startup dispatcher.
void bind_combat_runtime_factories(NativeStartupSources&, CombatRuntimeFactoryBindings&);
} // namespace skeet::client
