#pragma once
#include "skeet/engine/native_owner_registry_storage.hpp"
#include <cstdint>

namespace skeet::rage {
class SpreadStorage;
struct SpreadStorageBindings;
}
namespace skeet::network {
class DataUpdateObserverRegistry;
class TickShiftRechargeObserverRegistry;
}
namespace skeet::prediction {
struct ObserverRegistry;
}
namespace skeet {
class PreservedConfigStorage;
}
namespace skeet::client {
class CommandEmptyTickRegistry;
class CommandPhaseRegistry;
struct EntityObserverRegistry;

// The retained-image registry views are the same cells used by construction,
// dispatch, unload and ShutdownRuntime. Finalizers release their pointer
// buffers; they do not destroy borrowed owners or clear begin/end/limit.
struct StaticCleanupBindings {
    engine::NativeOwnerRegistryStorage& owner_registries;
    network::DataUpdateObserverRegistry& data_update_observers; //43475760/64/68
    network::TickShiftRechargeObserverRegistry& recharge_observers; //43475784/88/8C
    prediction::ObserverRegistry& prediction_observers; //43475AB0/B4/B8
    CommandEmptyTickRegistry& empty_tick_observers; //43475978/7C/80
    CommandPhaseRegistry& command_observers; //434757A0/A4/A8
    EntityObserverRegistry& entity_observers; //43475BA8/AC/B0
    rage::SpreadStorage& spread; //43478C60/43478C64, also consumed by aiming
    const rage::SpreadStorageBindings& spread_bindings;
    PreservedConfigStorage& configuration; //live43478CB8 baseline /43478C68 owner
    engine::JobMemory memory;
};

// This binding and every borrowed owner must outlive ExitCallbacks::run().
// Its no-argument callbacks share one process-lifetime source binding; no
// captured payload address is ever converted into an executable callback.
class StaticCleanupRuntime {
public:
    explicit StaticCleanupRuntime(StaticCleanupBindings);
    StaticCleanupRuntime(const StaticCleanupRuntime&) = delete;
    StaticCleanupRuntime& operator=(const StaticCleanupRuntime&) = delete;
    StaticCleanupRuntime(StaticCleanupRuntime&&) = delete;
    StaticCleanupRuntime& operator=(StaticCleanupRuntime&&) = delete;

    // Bind an initializer only when this reports an implemented finalizer
    // for its actual canonical storage. Other targets remain unmapped.
    bool has_source_callback(std::uint32_t native_target) const noexcept;
    void (*source_callback(std::uint32_t native_target) const)();
    void initialize_spread_storage() const; //43402E73, before registering4337D954
    engine::NativeOwnerRegistryStorage& owner_registry_storage() const noexcept {
        return bindings_.owner_registries;
    }

private:
    StaticCleanupBindings bindings_;
    void free_allocation(void*) const;
    void free_registry_storage(engine::OwnerRegistry&) const;
    static const StaticCleanupRuntime& bound();
    static void free_main_owners(); //433EB717
    static void free_hud_callbacks(); //43347D44
    static void free_network_callbacks(); //433F0746
    static void free_update_callbacks(); //433C9DB2
    static void free_packet_callbacks(); //433F4C95
    static void free_frame_callbacks(); //433D1E9F
    static void free_map_callbacks(); //433C9173
    static void free_blur_callbacks(); //43336AD4
    static void free_event_callbacks(); //43341B3F
    static void free_data_update_observers(); //433F4A1D
    static void free_recharge_observers(); //433E6194
    static void free_prediction_observers(); //433F515B
    static void free_empty_tick_observers(); //43349ECB
    static void free_command_observers(); //433E4A38
    static void free_entity_observers(); //433E4C98
    static void free_feedback_position_callbacks(); //433B8C06, buffer43475930
    static void free_spread_tables(); //4337D954 ->433AD1CD
    static void free_config_baseline(); //43329A5B
    static void free_config_settings(); //4332EFAD
};
}
