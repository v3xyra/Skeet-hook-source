#pragma once
#include "skeet/config_storage/preserved.hpp"
#include "skeet/engine/native_data_image.hpp"
#include "skeet/engine/interface_discovery.hpp"
#include "skeet/engine/build_module_bindings.hpp"
#include "skeet/engine/build_startup_convars.hpp"
#include "skeet/engine/build_animation_bindings.hpp"
#include "skeet/engine/build_command_bindings.hpp"
#include "skeet/engine/native_owner_registry_storage.hpp"
#include "skeet/engine/offset_storage.hpp"
#include "skeet/input/runtime.hpp"
#include "skeet/client/command_phase_registry.hpp"
#include "skeet/client/command_empty_tick.hpp"
#include "skeet/client/entity_observer_registry.hpp"
#include "skeet/client/shutdown.hpp"
#include "skeet/prediction/observer_registry.hpp"
#include "skeet/network/data_update_observers.hpp"
#include "skeet/network/tick_shift_observers.hpp"
#include "skeet/events/registry.hpp"
#include "skeet/menu/settings_menu_storage.hpp"
#include "skeet/menu/startup_data.hpp"
#include <memory>

namespace skeet::client {
class PlayerRuntimeOwner;
class MiscRuntimeOwner;
struct EngineRuntimeBindings {
    engine::BuildModuleBases modules;
    engine::BuildEngineInterfaces interfaces;
    engine::BuildStartupConVarBindings variables;
    animation::BoneAttachmentUpdate update_bone_attachments;
    engine::BuildCommandBindings command{};
};
// Concrete storage shared by startup factories, dispatchers and shutdown.
// The default constructor creates one zeroed source data image and obtains
// the actual Windows process-heap services. No executable image or VM is
// required to construct these canonical source instances.
class ClientRuntimeStorage {
public:
    ClientRuntimeStorage();
    explicit ClientRuntimeStorage(std::shared_ptr<engine::NativeDataImage>);
    ~ClientRuntimeStorage();
    ClientRuntimeStorage(const ClientRuntimeStorage&) = delete;
    ClientRuntimeStorage& operator=(const ClientRuntimeStorage&) = delete;
    ClientRuntimeStorage(ClientRuntimeStorage&&) = delete;
    ClientRuntimeStorage& operator=(ClientRuntimeStorage&&) = delete;

    engine::NativeDataImage& data() const noexcept {
        return *data_;
    }
    std::shared_ptr<engine::NativeDataImage> data_owner() const noexcept {
        return data_;
    }
    engine::JobMemory memory() const noexcept {
        return memory_;
    }
    PreservedConfigStorage& config_storage() noexcept {
        return *configuration_;
    }
    std::shared_ptr<PreservedConfigStorage> config_storage_owner() const noexcept {
        return configuration_;
    }
    ConfigRegistries& configuration() noexcept {
        return configuration_->configuration();
    }
    RegisteredHotkey& menu_hotkey() const;
    ui::SettingsMenuStorage& settings_menu_storage() noexcept {
        return settings_menu_;
    }
    ui::MenuStartupData& menu_startup_data() noexcept {
        return menu_data_;
    }
    input::Runtime& input_runtime() noexcept {
        return input_;
    }
    engine::NativeOwnerRegistryStorage& owner_registries() noexcept {
        return owner_registries_;
    }
    CommandPhaseRegistry& command_phases() noexcept {
        return commands_;
    }
    CommandEmptyTickRegistry& empty_ticks() noexcept {
        return empty_ticks_;
    }
    EntityObserverRegistry& entity_observers() noexcept {
        return entities_;
    }
    prediction::ObserverRegistry& prediction_observers() noexcept {
        return predictions_;
    }
    network::DataUpdateObserverRegistry& data_update_observers() noexcept {
        return data_updates_;
    }
    network::TickShiftRechargeObserverRegistry& recharge_observers() noexcept {
        return recharge_;
    }
    engine::NativeOffsetStorage& offsets() noexcept {
        return offsets_;
    }
    events::EventNameStorage& event_names() noexcept {
        return event_names_;
    }
    events::EventRegistry& event_registry() noexcept {
        return events_;
    }
    ExitCallbacks& exit_callbacks() noexcept {
        return exits_;
    }
    // Acquire the seven proven engine interfaces before constructing owners
    // that borrow them. A failed acquisition publishes no partial bundle.
    engine::CoreEngineInterfaces bind_core_interfaces(engine::FactoryProvider&);
    engine::CoreEngineInterfaces bind_core_interfaces();
    // Call after interface discovery and HUD lookup publication, before input hooks.
    void bind_input_interfaces();

    // Construct the concrete engine inputs consumed by source owners: loaded
    // modules, interfaces, receive/datamap offsets and live ConVar references.
    // This phase runs once per storage instance. A failed attempt requires a
    // new instance; partially initialized offset keys must not be XORed twice.
    // CRT registration, feature owners, hooks and menu startup are separate.
    EngineRuntimeBindings initialize_engine_bindings(const engine::SourceInitializationImports&,
                                                     std::uint32_t timestamp_low);

    // Storage construction performs no CRT initializers, owner registration,
    // engine-interface binding or hook installation. Those operations retain
    // their actual source startup order and borrow these same instances.
    // Keep this aggregate alive until owners and terminal callbacks finish;
    // Its C++ destruction releases source offset storage if terminal cleanup
    // has not already cleared it; it does not run other native finalizers.
private:
    friend class PlayerRuntimeOwner;
    friend class MiscRuntimeOwner;
    PlayerRuntimeOwner* active_player_owner_ = nullptr;
    MiscRuntimeOwner* active_misc_owner_ = nullptr;
    std::shared_ptr<engine::NativeDataImage> data_;
    engine::JobMemory memory_;
    std::shared_ptr<PreservedConfigStorage> configuration_;
    ui::SettingsMenuStorage settings_menu_;
    ui::MenuStartupData menu_data_;
    input::Runtime input_;
    engine::NativeOwnerRegistryStorage owner_registries_;
    CommandPhaseRegistry commands_;
    CommandEmptyTickRegistry empty_ticks_;
    EntityObserverRegistry entities_;
    prediction::ObserverRegistry predictions_;
    network::DataUpdateObserverRegistry data_updates_;
    network::TickShiftRechargeObserverRegistry recharge_;
    engine::NativeOffsetStorage offsets_;
    events::EventNameStorage event_names_;
    events::EventRegistry events_;
    ExitCallbacks exits_;
    bool engine_bindings_started_ = false;
};
} //namespace skeet::client
