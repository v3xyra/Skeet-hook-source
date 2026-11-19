#include "skeet/client/static_cleanup.hpp"
#include "skeet/engine/unload.hpp"
#include "skeet/rage/spread_storage.hpp"
#include "skeet/config_storage/preserved.hpp"
#include "skeet/network/data_update_observers.hpp"
#include "skeet/network/tick_shift_observers.hpp"
#include "skeet/prediction/observer_registry.hpp"
#include "skeet/client/command_empty_tick.hpp"
#include "skeet/client/command_phase_registry.hpp"
#include "skeet/client/entity_observer_registry.hpp"
#include <stdexcept>

namespace skeet::client {
namespace {
const StaticCleanupRuntime* active_cleanup = nullptr;
}

StaticCleanupRuntime::StaticCleanupRuntime(StaticCleanupBindings bindings) : bindings_(bindings) {
    if (!bindings_.memory.free || !bindings_.memory.process_heap)
        throw std::invalid_argument("Static cleanup requires the canonical process heap binding");
    const auto* const image = &bindings_.owner_registries.image();
    if (&bindings_.spread.image() != image || &bindings_.configuration.image() != image ||
        &bindings_.data_update_observers.image() != image ||
        &bindings_.recharge_observers.image() != image ||
        &bindings_.prediction_observers.image() != image ||
        &bindings_.empty_tick_observers.image() != image ||
        &bindings_.command_observers.image() != image ||
        &bindings_.entity_observers.image() != image)
        throw std::invalid_argument(
            "Static cleanup storage must retain the same initialization image");
}

bool StaticCleanupRuntime::has_source_callback(std::uint32_t target) const noexcept {
    switch (target) {
    case 0x433EB717u:
    case 0x43347D44u:
    case 0x433F0746u:
    case 0x433C9DB2u:
    case 0x433F4C95u:
    case 0x433D1E9Fu:
    case 0x433C9173u:
    case 0x43336AD4u:
    case 0x43341B3Fu:
    case 0x4337D954u:
    case 0x43329A5Bu:
    case 0x4332EFADu:
    case 0x433F4A1Du:
    case 0x433B8C06u:
    case 0x433E6194u:
    case 0x433F515Bu:
    case 0x43349ECBu:
    case 0x433E4A38u:
    case 0x433E4C98u:
        return true;
    default:
        return false;
    }
}

void (*StaticCleanupRuntime::source_callback(std::uint32_t target) const)() {
    if (!has_source_callback(target))
        throw std::out_of_range("Static finalizer has no concrete source storage owner");
    if (active_cleanup && active_cleanup != this)
        throw std::logic_error("Exit callbacks require one stable static cleanup owner");
    active_cleanup = this;
    switch (target) {
    case 0x433EB717u:
        return free_main_owners;
    case 0x43347D44u:
        return free_hud_callbacks;
    case 0x433F0746u:
        return free_network_callbacks;
    case 0x433C9DB2u:
        return free_update_callbacks;
    case 0x433F4C95u:
        return free_packet_callbacks;
    case 0x433D1E9Fu:
        return free_frame_callbacks;
    case 0x433C9173u:
        return free_map_callbacks;
    case 0x43336AD4u:
        return free_blur_callbacks;
    case 0x43341B3Fu:
        return free_event_callbacks;
    case 0x4337D954u:
        return free_spread_tables;
    case 0x43329A5Bu:
        return free_config_baseline;
    case 0x4332EFADu:
        return free_config_settings;
    case 0x433F4A1Du:
        return free_data_update_observers;
    case 0x433E6194u:
        return free_recharge_observers;
    case 0x433F515Bu:
        return free_prediction_observers;
    case 0x43349ECBu:
        return free_empty_tick_observers;
    case 0x433E4A38u:
        return free_command_observers;
    case 0x433E4C98u:
        return free_entity_observers;
    case 0x433B8C06u:
        return free_feedback_position_callbacks;
    default:
        throw std::out_of_range("Static finalizer has no concrete source storage owner");
    }
}

const StaticCleanupRuntime& StaticCleanupRuntime::bound() {
    if (!active_cleanup)
        throw std::logic_error("Static cleanup source binding is not installed");
    return *active_cleanup;
}

void StaticCleanupRuntime::free_allocation(void* allocation) const {
    engine::UnloadSystemFunctions memory;
    memory.free_heap = bindings_.memory.free;
    memory.process_heap = bindings_.memory.process_heap;
    // Native callbacks load their live begin slot once, null-test it, then
    // tail-call433837D0. That existing body restores the allocation base.
    // No registry pointer is reset and no registered object is freed here.
    engine::free_native_allocation(allocation, memory);
}
void StaticCleanupRuntime::free_registry_storage(engine::OwnerRegistry& registry) const {
    free_allocation(registry.begin);
}

void StaticCleanupRuntime::free_main_owners() {
    const auto& runtime = bound();
    runtime.free_registry_storage(runtime.bindings_.owner_registries.at(0x43475B78u));
}
void StaticCleanupRuntime::free_hud_callbacks() {
    const auto& runtime = bound();
    runtime.free_registry_storage(runtime.bindings_.owner_registries.at(0x43475A9Cu));
}
void StaticCleanupRuntime::free_network_callbacks() {
    const auto& runtime = bound();
    runtime.free_registry_storage(runtime.bindings_.owner_registries.at(0x43475B94u));
}
void StaticCleanupRuntime::free_update_callbacks() {
    const auto& runtime = bound();
    runtime.free_registry_storage(runtime.bindings_.owner_registries.at(0x43475988u));
}
void StaticCleanupRuntime::free_packet_callbacks() {
    const auto& runtime = bound();
    runtime.free_registry_storage(runtime.bindings_.owner_registries.at(0x4347584Cu));
}
void StaticCleanupRuntime::free_frame_callbacks() {
    const auto& runtime = bound();
    runtime.free_registry_storage(runtime.bindings_.owner_registries.at(0x43475AD8u));
}
void StaticCleanupRuntime::free_map_callbacks() {
    const auto& runtime = bound();
    runtime.free_registry_storage(runtime.bindings_.owner_registries.at(0x43475770u));
}
void StaticCleanupRuntime::free_blur_callbacks() {
    const auto& runtime = bound();
    runtime.free_registry_storage(runtime.bindings_.owner_registries.at(0x43475A84u));
}
void StaticCleanupRuntime::free_event_callbacks() {
    const auto& runtime = bound();
    runtime.free_registry_storage(runtime.bindings_.owner_registries.at(0x434758E0u));
}
void StaticCleanupRuntime::free_data_update_observers() {
    bound().bindings_.data_update_observers.finalize_storage();
}
void StaticCleanupRuntime::free_recharge_observers() {
    bound().bindings_.recharge_observers.finalize_storage();
}
void StaticCleanupRuntime::free_prediction_observers() {
    bound().bindings_.prediction_observers.finalize_storage();
}
void StaticCleanupRuntime::free_empty_tick_observers() {
    bound().bindings_.empty_tick_observers.finalize_storage();
}
void StaticCleanupRuntime::free_command_observers() {
    bound().bindings_.command_observers.finalize_storage();
}
void StaticCleanupRuntime::free_entity_observers() {
    bound().bindings_.entity_observers.finalize_storage();
}
void StaticCleanupRuntime::free_feedback_position_callbacks() {
    const auto& runtime = bound();
    runtime.free_registry_storage(runtime.bindings_.owner_registries.at(0x43475930u));
}
void StaticCleanupRuntime::initialize_spread_storage() const {
    bindings_.spread.initialize(bindings_.spread_bindings);
}
void StaticCleanupRuntime::free_spread_tables() {
    const auto& runtime = bound();
    runtime.bindings_.spread.finalize(runtime.bindings_.spread_bindings);
}
void StaticCleanupRuntime::free_config_baseline() {
    const auto& runtime = bound();
    runtime.bindings_.configuration.finalize_baseline(runtime.bindings_.memory);
}
void StaticCleanupRuntime::free_config_settings() {
    const auto& runtime = bound();
    runtime.bindings_.configuration.finalize_settings(runtime.bindings_.memory);
}
}
