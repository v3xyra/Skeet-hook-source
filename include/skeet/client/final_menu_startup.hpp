#pragma once
#include "skeet/menu/startup_registry.hpp"
#include <stdexcept>

namespace skeet::misc {
class SettingsRuntimeOwner;
}
namespace skeet::ui {
class MainMenuWindowOwner;
class StartupRenderer;
}
namespace skeet::client {
class ClientRuntimeStorage;

// Publish the full feature-registry baseline after the ten private settings
// descriptors have been removed. The canonical NativeConfigVector owns the
// same aligned process-heap allocation used by its terminal finalizer.
void capture_startup_configuration_baseline(ClientRuntimeStorage&);

// The main initializer calls this after actual RootMenu/default-layout
// construction, before lifecycle owner slot1 and static patch installation.
// This phase allocates no second menu, settings registry or configuration.
class FinalMenuStartupTail {
public:
    FinalMenuStartupTail(ClientRuntimeStorage&, ui::MainMenuWindowOwner&,
                         misc::SettingsRuntimeOwner&, ConfigStorageOwner&, ui::StartupRenderer&);
    void initialize();
    bool complete() const noexcept {
        return complete_;
    }

private:
    ClientRuntimeStorage& storage_;
    ui::MainMenuWindowOwner& window_;
    misc::SettingsRuntimeOwner& settings_;
    ConfigStorageOwner& configuration_;
    ui::StartupRenderer& renderer_;
    bool started_ = false, complete_ = false;
};
} //namespace skeet::client
