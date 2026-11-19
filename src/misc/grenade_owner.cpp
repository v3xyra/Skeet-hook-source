#include "skeet/misc/grenade_owner.hpp"
#include "skeet/engine/scene_queries.hpp"
#include <algorithm>
#include <array>
#include <cstring>
#include <new>

namespace skeet::misc {
namespace {
#if defined(_M_IX86) || defined(__i386__)
#define GRENADE_MEMBER __thiscall
#define GRENADE_FAST __fastcall
#else
#define GRENADE_MEMBER
#define GRENADE_FAST
#endif
using OwnerEntry = void(GRENADE_MEMBER*)(void*);
class GrenadeOwner;
struct HudEndpoint : hud::HudFrameCallback {
    GrenadeOwner* owner;
};
struct UpdateEndpoint : network::UpdateObserver {
    GrenadeOwner* owner;
};
void** active_slot = nullptr;
void GRENADE_MEMBER destruction(void*);
void GRENADE_MEMBER release_material(void*);
void GRENADE_MEMBER reset(void*);
void GRENADE_MEMBER forward_reset(void*);
void GRENADE_MEMBER forward_seven(void*);
void GRENADE_MEMBER unchanged(void*) {} //433CCE8E
void GRENADE_MEMBER ribbons(void*);
void GRENADE_FAST hud_frame(void*, ui::RenderQueue*, void*, std::int32_t*);
void GRENADE_FAST unchanged_player(void*, void*, std::uintptr_t, std::uintptr_t, std::uintptr_t) {
} //4337D653
void GRENADE_FAST unchanged_view(void*, float*, float*, float*) {} //4334A269
void GRENADE_FAST unchanged_post(void*, void*) {} //433CCE8E
const std::array<OwnerEntry, 9> owner_table{destruction, unchanged,        unchanged,
                                            unchanged,   release_material, reset,
                                            unchanged,   forward_reset,    forward_seven};
const std::array<hud::HudFrameEntry, 1> hud_table{hud_frame};
const network::UpdateObserverVtable update_table{
    {unchanged, unchanged, unchanged, ribbons, unchanged},
    unchanged_player,
    unchanged_view,
    unchanged_post};

template <class T> T& object(std::byte* storage) noexcept {
    return *std::launder(reinterpret_cast<T*>(storage));
}
class GrenadeOwner {
public:
    const OwnerEntry* vtable = nullptr;
    GrenadeOwnerBindings bindings;
    engine::JobMemory memory;
    GrenadePredictionConfig& settings;
    esp::GrenadeEspConfig& visual_settings;
    esp::GrenadeWarningOwner warnings;
    esp::GrenadeTrajectoryOwner trajectory;
    GrenadeCommandMovement movement;
    client::NativeCommandPhaseReceiver command_receiver{};
    HudEndpoint hud{{nullptr}, this};
    client::NativeEntityObserverReceiver entity_receiver{};
    UpdateEndpoint update{{nullptr}, this};
    network::NativeTickShiftRechargeReceiver recharge_receiver{};
    client::NativeCommandEmptyTickReceiver empty_tick_receiver{};
    alignas(
        esp::GrenadeLiveScene) std::array<std::byte, sizeof(esp::GrenadeLiveScene)> live_storage;
    alignas(GrenadeCommandScene) std::array<std::byte, sizeof(GrenadeCommandScene)> scene_storage;
    alignas(
        GrenadeCommandRuntime) std::array<std::byte, sizeof(GrenadeCommandRuntime)> command_storage;
    alignas(effects::ProjectileRibbonOwner)
        std::array<std::byte, sizeof(effects::ProjectileRibbonOwner)> ribbon_storage;
    esp::GrenadeLiveScene& live() noexcept {
        return object<esp::GrenadeLiveScene>(live_storage.data());
    }
    GrenadeCommandScene& scene() noexcept {
        return object<GrenadeCommandScene>(scene_storage.data());
    }
    GrenadeCommandRuntime& command() noexcept {
        return object<GrenadeCommandRuntime>(command_storage.data());
    }
    effects::ProjectileRibbonOwner& ribbon() noexcept {
        return object<effects::ProjectileRibbonOwner>(ribbon_storage.data());
    }

    GrenadeOwner(const GrenadeOwnerRegistries& registries, ConfigRegistries& configuration,
                 GrenadePredictionConfig& canonical, esp::GrenadeEspConfig& visual,
                 GrenadeOwnerBindings input)
        : bindings(input), memory(registries.lifecycle.memory), settings(canonical),
          visual_settings(visual),
          movement{bindings.live.movement_capture, bindings.live.movement_environment} {
        auto* const command_address =
            reinterpret_cast<GrenadeCommandRuntime*>(command_storage.data());
        registries.lifecycle.append(this); //4348676F
        registries.commands.append(command_receiver, command_address); //43486777
        registries.empty_ticks.append_grenade(empty_tick_receiver, command_address); //4348677F
        hud::register_hud_frame(registries.hud, &hud); //43486787
        registries.entities.append_grenades(entity_receiver, command_address); //4348678F
        network::register_update_observer(registries.updates, update); //43486797
        registries.recharge.append_noop(recharge_receiver, command_address); //4348679F
        registries.entities.publish_grenades(entity_receiver); //434867A6, table4346DC78
        update.vtable = &update_table; //434867AF
        registries.recharge.publish_noop(recharge_receiver); //434867B6, table434727FC
        vtable = owner_table.data(); //434867C0
        registries.commands.publish(command_receiver,
                                    client::CommandOwnerId::owner_4346f0ac); //434867C6
        registries.empty_ticks.publish_grenade(empty_tick_receiver); //434867CD
        hud.vtable = hud_table.data(); //434867D4
        initialize_grenade_prediction_config(configuration, settings, visual_settings);
        ::new (live_storage.data()) esp::GrenadeLiveScene(bindings.live);
        ::new (scene_storage.data()) GrenadeCommandScene{
            live(), movement, bindings.release_movement, bindings.movement_feedback};
        const GrenadeCommandBindings engine{
            bindings.live.queries.grenades().bindings().entity_list,
            bindings.input,
            bindings.live.convars.recoil_scale,
            capture_grenade_command_offsets(bindings.live.queries.offsets()),
            &bindings.command_tick,
            &bindings.live.queries.environment().tick_interval,
            &bindings.live.current_time,
            &bindings.release_choke_enabled,
            &bindings.observer_suppressed,
            &bindings.live.command_buttons,
            bindings.view_angles,
            &visual_settings,
            &warnings,
            scene()};
        ::new (command_storage.data()) GrenadeCommandRuntime(settings, engine);
        ::new (ribbon_storage.data()) effects::ProjectileRibbonOwner;
        ribbon().material_names = {"$spriterendermode", "particle/beam_taser", "$basetexture",
                                   "Sprite"};
    }
    ~GrenadeOwner() {
        //4340BC4A: four strings first, then F0/E0/D0/BC vectors in this order.
        ribbon().~ProjectileRibbonOwner();
        std::vector<esp::GrenadeScreenPoint>{}.swap(trajectory.path.bounces);
        std::vector<esp::GrenadeScreenPoint>{}.swap(trajectory.path.points);
        std::vector<esp::Vector3>{}.swap(warnings.projectile_points);
        std::vector<esp::GrenadeWarningRecord>{}.swap(warnings.records);
        command().~GrenadeCommandRuntime();
        scene().~GrenadeCommandScene();
        live().~GrenadeLiveScene();
    }
};
void GRENADE_MEMBER destruction(void* storage) {
    destroy_grenade_owner(storage, *active_slot);
}
void GRENADE_MEMBER release_material(void* storage) {
    static_cast<GrenadeOwner*>(storage)->ribbon().release_material();
}
void GRENADE_MEMBER reset(void* storage) {
    auto& owner = *static_cast<GrenadeOwner*>(storage);
    std::memset(&owner.command().state.released_command, 0, sizeof(client::Command));
    owner.command().state.release_command_number = 0;
    owner.warnings.records.clear();
}
void GRENADE_MEMBER forward_reset(void* storage) {
    auto& owner = *static_cast<GrenadeOwner*>(storage);
    owner.vtable[5](storage);
}
void GRENADE_MEMBER forward_seven(void* storage) {
    auto& owner = *static_cast<GrenadeOwner*>(storage);
    owner.vtable[7](storage);
}
void GRENADE_MEMBER ribbons(void* endpoint) {
    auto& owner =
        *static_cast<UpdateEndpoint*>(static_cast<network::UpdateObserver*>(endpoint))->owner;
    const auto& b = owner.bindings;
    const effects::ProjectileRibbonLiveFrame frame{
        b.ribbons_suppressed, b.live.grenade_entities_enabled, b.live.tick_count, b.ribbons_color};
    owner.ribbon().draw(frame, owner.warnings.records, b.ribbons, b.live.queries.environment(),
                        owner.live().trace(), owner.warnings.projectile_points);
}
void GRENADE_FAST hud_frame(void* endpoint, ui::RenderQueue* queue, void*, std::int32_t*) {
    auto& owner = *static_cast<HudEndpoint*>(static_cast<hud::HudFrameCallback*>(endpoint))->owner;
    owner.live().draw_prediction(*queue, owner.settings, owner.visual_settings, owner.trajectory,
                                 owner.command().state, owner.warnings);
}
void* allocate(engine::JobMemory memory, std::size_t size) {
    auto* raw = memory.allocate(memory.process_heap, 8, size + 0x20);
    if (!raw)
        return nullptr;
    const auto base = reinterpret_cast<std::uintptr_t>(raw);
    const auto header = (base + 15) & ~std::uintptr_t(15);
    const auto displacement = std::uint32_t(header - base), total = std::uint32_t(size + 0x20);
    std::memcpy(reinterpret_cast<void*>(header + 8), &displacement, 4);
    std::memcpy(reinterpret_cast<void*>(header + 12), &total, 4);
    return reinterpret_cast<void*>(header + 16);
}
}
std::size_t grenade_owner_size() noexcept {
    return sizeof(GrenadeOwner);
}
void construct_grenade_owner(void* storage, const GrenadeOwnerRegistries& registries,
                             ConfigRegistries& config, GrenadePredictionConfig& settings,
                             esp::GrenadeEspConfig& visual, GrenadeOwnerBindings bindings) {
    active_slot = &bindings.active;
    ::new (storage) GrenadeOwner(registries, config, settings, visual, bindings);
    bindings.active = storage;
}
void* create_grenade_owner(const GrenadeOwnerRegistries& registries, ConfigRegistries& config,
                           GrenadePredictionConfig& settings, esp::GrenadeEspConfig& visual,
                           GrenadeOwnerBindings bindings) {
    auto* storage =
        allocate(registries.lifecycle.memory, std::max(std::size_t(0x2A8), grenade_owner_size()));
    if (storage)
        construct_grenade_owner(storage, registries, config, settings, visual, bindings);
    else
        bindings.active = nullptr;
    return storage;
}
void destroy_grenade_owner(void* storage, void*& active) {
    active = nullptr;
    if (!storage)
        return;
    auto* owner = static_cast<GrenadeOwner*>(storage);
    const auto memory = owner->memory;
    owner->~GrenadeOwner();
    auto* bytes = static_cast<std::byte*>(storage);
    std::uint32_t displacement;
    std::memcpy(&displacement, bytes - 8, 4);
    memory.free(memory.process_heap, 0, bytes - 16 - displacement);
}
GrenadeCommandRuntime& grenade_owner_command(void* storage) noexcept {
    return static_cast<GrenadeOwner*>(storage)->command();
}
esp::GrenadeLiveScene& grenade_owner_scene(void* storage) noexcept {
    return static_cast<GrenadeOwner*>(storage)->live();
}
std::span<const esp::GrenadeWarningRecord>
grenade_owner_warning_records(const void* storage) noexcept {
    return static_cast<const GrenadeOwner*>(storage)->warnings.records;
}
#undef GRENADE_MEMBER
#undef GRENADE_FAST
} // namespace skeet::misc
