#include "skeet/client/startup_bindings.hpp"
#include "skeet/client/runtime_storage.hpp"
#include "skeet/config_storage/owner.hpp"
#include "skeet/input/window_hook.hpp"
#include "skeet/menu/root_menu.hpp"
#include "skeet/menu/startup_window.hpp"
#include "skeet/render/startup_renderer.hpp"
#include "skeet/script/owner.hpp"
#include <array>
#include <stdexcept>
#include <utility>

namespace skeet::client {
PostInitializationData::PostInitializationData(std::shared_ptr<engine::NativeDataImage> data)
    : data_(std::move(data)) {
    (void)engine::require_native_data_image(data_);
}

PostInitializationState PostInitializationData::state() const {
    auto& d = *data_;
    return {d.native_cell<StartupFactory>(0x434678C4u),
            d.native_cell<void*>(0x43467460u),
            d.native_cell<StartupFactory>(0x43468B9Cu),
            d.native_cell<void*>(0x43476424u),
            d.native_cell<StartupRefreshSource*>(0x43467C04u),
            d.native_cell<std::uint8_t>(0x43475BD2u),
            d.native_cell<std::uint8_t>(0x434758B0u)};
}
CommandResetState PostInitializationData::command_reset() const {
    auto& d = *data_;
    return {d.native_cell<std::uint32_t>(0x434759C0u),
            d.native_cell<std::int32_t>(0x434759C4u),
            d.native_cell<std::int32_t>(0x434759C8u),
            d.native_cell<std::int32_t>(0x434759CCu),
            d.native_cell<std::int32_t>(0x434759DCu),
            d.native_cell<std::int32_t>(0x434759E8u),
            d.native_cell<std::int32_t>(0x434759F8u),
            d.native_cell<std::array<std::uint32_t, 150>>(0x43476090u),
            {d.native_cell<std::int32_t>(0x4346DB1Cu), d.native_cell<std::uint32_t>(0x4346DB2Cu),
             d.native_cell<std::uint32_t>(0x4346DB30u), d.native_cell<std::uint32_t>(0x4346DB38u)}};
}
std::span<StartupAcknowledgement, 150> PostInitializationData::acknowledgements() const {
    return data_->native_cell<std::array<StartupAcknowledgement, 150>>(0x43475BE0u);
}
PostInitializationImports native_post_initialization_imports() {
    return {native_startup_imports(), ui::native_window_lifetime_imports()};
}

namespace {
PostInitializationBindings
compose_bindings(PostInitializationData& data, ClientRuntimeStorage& storage,
                 const EngineRuntimeBindings& engine, ui::MainMenuWindowOwner& window,
                 ui::StartupRenderer& renderer, ui::IconCache& icons,
                 ui::StartupIcons& startup_icons, input::WindowSubclassRuntime& subclass,
                 ConfigStorageOwner& configuration, script::ScriptOwner& scripts,
                 ShutdownRuntime& finalizer, PostInitializationImports imports) {
    auto& d = storage.data();
    auto& menu = window.controller();
    auto& hooks = subclass.hooks.state;
    if (&data.data() != &d || &engine.variables.data() != &d ||
        &window.menu_data() != &storage.menu_startup_data() ||
        &renderer.dpi() != &storage.menu_startup_data().dpi ||
        d.native_cell<ui::RendererContainer*>(0x4347690Cu) != &renderer.renderer() ||
        d.native_cell<ui::RootMenu*>(0x43476924u) != &menu || &menu.window() != &window.window() ||
        &configuration.native_storage() != &storage.config_storage() ||
        &configuration.configuration() != &storage.configuration() ||
        !configuration.menu_bound_to(menu) ||
        d.native_cell<script::ScriptOwner*>(0x43475B00u) != &scripts ||
        &subclass.input != &storage.input_runtime() ||
        &finalizer.exit_callbacks != &storage.exit_callbacks() ||
        &finalizer.scripts != &scripts.modules().shutdown_state() ||
        hooks.engine != d.native_cell<void*>(0x4346A514u) || !hooks.engine ||
        hooks.event_manager != d.native_cell<void*>(0x4346A470u) || !hooks.event_manager)
        throw std::invalid_argument(
            "Post-initialization requires the canonical completed process owners");
    if ((finalizer.main_callbacks && finalizer.main_callbacks != &window.callbacks()) ||
        (finalizer.renderer && finalizer.renderer != &renderer.renderer()))
        throw std::invalid_argument(
            "Post-initialization cannot replace a different GUI shutdown owner");
    if (!imports.startup.get_command_line_a || !imports.window_lifetime.free_unicode_string)
        throw std::invalid_argument(
            "Post-initialization requires its actual startup and window imports");
    return {hooks,
            renderer.device_listener(),
            menu,
            icons,
            startup_icons,
            subclass,
            data.command_reset(),
            data.acknowledgements(),
            window.startup_scripts(),
            configuration,
            scripts,
            finalizer};
}
}

PostInitializationOwner::PostInitializationOwner(
    ClientRuntimeStorage& storage, const EngineRuntimeBindings& engine,
    ui::MainMenuWindowOwner& window, ui::StartupRenderer& renderer, ui::IconCache& icons,
    ui::StartupIcons& startup_icons, input::WindowSubclassRuntime& subclass,
    ConfigStorageOwner& configuration, script::ScriptOwner& scripts, ShutdownRuntime& finalizer)
    : PostInitializationOwner(storage, engine, window, renderer, icons, startup_icons, subclass,
                              configuration, scripts, finalizer,
                              native_post_initialization_imports()) {}

PostInitializationOwner::PostInitializationOwner(
    ClientRuntimeStorage& storage, const EngineRuntimeBindings& engine,
    ui::MainMenuWindowOwner& window, ui::StartupRenderer& renderer, ui::IconCache& icons,
    ui::StartupIcons& startup_icons, input::WindowSubclassRuntime& subclass,
    ConfigStorageOwner& configuration, script::ScriptOwner& scripts, ShutdownRuntime& finalizer,
    PostInitializationImports imports)
    : data_(storage.data_owner()), menu_(window.controller()),
      runtime_(data_.state(), engine.variables.startup_variables(),
               compose_bindings(data_, storage, engine, window, renderer, icons, startup_icons,
                                subclass, configuration, scripts, finalizer, imports),
               imports.startup) {
    finalizer.main_callbacks = &window.callbacks();
    finalizer.renderer = &renderer.renderer();
    finalizer.window_imports = imports.window_lifetime;
}
} //namespace skeet::client
