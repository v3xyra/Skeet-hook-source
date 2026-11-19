#include "skeet/client/startup.hpp"
#include "skeet/client/shutdown.hpp"
#include "skeet/config_storage/owner.hpp"
#include "skeet/config_storage/preserved.hpp"
#include "skeet/config_storage/presets.hpp"
#include "skeet/engine/interface_call.hpp"
#include "skeet/engine/unload.hpp"
#include "skeet/input/window_hook.hpp"
#include "skeet/menu/root_menu.hpp"
#include "skeet/menu/skins_menu.hpp"
#include "skeet/render/device_listener.hpp"
#include "skeet/render/startup_icons.hpp"
#include "skeet/script/owner.hpp"
#include <cstring>
#include <stdexcept>
#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

namespace skeet::client {
namespace {
#if defined(_WIN32)
const char* __stdcall command_line_a() {
    return GetCommandLineA();
}
#endif
}

StartupImports native_startup_imports() {
#if defined(_WIN32)
    return {command_line_a};
#else
    throw std::runtime_error("Client startup imports require Windows");
#endif
}

void install_startup_window(ui::MenuWindow& menu, input::WindowSubclassRuntime& window) {
    if (!menu.native_window)
        throw std::invalid_argument("Client startup main window is unresolved");
    if (window.input.main_menu != &menu)
        throw std::invalid_argument("Client startup and input must share the main menu window");
    window.install(menu.native_window);
}

PostInitializationRuntime::PostInitializationRuntime(PostInitializationState state,
                                                     StartupConVars variables,
                                                     PostInitializationBindings bindings,
                                                     StartupImports imports)
    : state_(state), variables_(variables), bindings_(bindings), imports_(imports) {
    if (&bindings_.menu.script_owner() != &bindings_.scripts)
        throw std::invalid_argument("Client startup must use the main menu's script owner");
    if (&bindings_.scripts.modules().unload_state() != &bindings_.hooks)
        throw std::invalid_argument(
            "Client startup must use the script module owner's unload state");
    if (&bindings_.finalizer.window_title !=
        &bindings_.configuration_storage.native_storage().path())
        throw std::invalid_argument(
            "Client shutdown and CONFIG must share the native preset-store path cell");
    if (&bindings_.finalizer.native_window != &bindings_.menu.native_window())
        throw std::invalid_argument("Client startup and shutdown must share the main window cell");
    if (!bindings_.window.input.main_menu ||
        &bindings_.window.input.main_menu->native_window != &bindings_.menu.native_window())
        throw std::invalid_argument("Client startup and input must share the main menu window");
    if (!bindings_.menu.native_window())
        throw std::invalid_argument("Client startup requires the resolved main window");
    if (&bindings_.finalizer.input != &bindings_.window.input ||
        &bindings_.window.hooks.state != &bindings_.hooks ||
        &bindings_.finalizer.hooks.state != &bindings_.hooks)
        throw std::invalid_argument("Client window lifecycle must share input and hook state");
    if (!bindings_.configuration_storage.menu_bound_to(bindings_.menu))
        throw std::invalid_argument(
            "Client startup requires the composed CONFIG main menu binding");
    // Main-menu composition binds CONFIG once before this runtime exists.
    // The script finalizer attachment is separate from4341F57F call order.
    bindings_.scripts.bind_shutdown(bindings_.finalizer);
    // Bind the recovered static listener identity for both startup and cleanup.
    bindings_.hooks.event_listener = &bindings_.renderer_listener;
}

void PostInitializationRuntime::configure_variables() { // 4341F652..4341F740
    engine::invoke<void>(variables_.at_43467fbc, 16, 0xc0000);
    engine::invoke<void>(variables_.at_43467130, 16, 0x4b0);
    engine::invoke<void>(variables_.at_4346a5b4, 16, 0x40);
    engine::invoke<void>(variables_.at_43468ba8, 16, 0);
    if (engine::invoke<bool>(bindings_.hooks.engine, 27))
        return;

    engine::invoke<void>(variables_.at_434671ac, 16, 0x40);
    engine::invoke<void>(variables_.at_4346747c, 16, 0x40);
    auto* const callback_count = static_cast<std::byte*>(variables_.at_4346a748.get()) + 0x60;
    std::uint32_t saved;
    std::memcpy(&saved, callback_count, sizeof(saved));
    const std::uint32_t zero = 0;
    std::memcpy(callback_count, &zero, sizeof(zero));
    engine::invoke<void>(variables_.at_4346a748, 15, 1.0f / 64.0f);
    std::memcpy(callback_count, &saved, sizeof(saved));
    engine::invoke<void>(variables_.at_4346a330, 16, 2);

    auto* const source = state_.refresh_source;
    if (!source)
        return;
    auto* const vector = source->vector;
    if (!vector)
        return;
    for (std::int32_t i = 0; i < vector->count; ++i)
        engine::invoke<void>(vector->objects[i], 0);
}

void PostInitializationRuntime::initialize() { // 4341F57F..4341F7CD
    auto& hooks = bindings_.hooks;
    if (!hooks.listener_registered) {
        hooks.listener_registered = true;
        engine::invoke<void>(hooks.event_manager, 20, &bindings_.renderer_listener);
    }
    if (const auto factory = state_.first_factory) {
        state_.first_result = factory();
        state_.first_factory = nullptr;
    }
    if (const auto factory = state_.second_factory)
        state_.second_result = factory();

    configure_variables();
    bindings_.menu.skins_menu().initialize_default_paints();
    ui::initialize_startup_icons(bindings_.icons, bindings_.startup_icons);
    auto objects = hooks.owners.entries();
    for (auto* object : objects)
        engine::invoke<void>(object, 2);

    bindings_.menu.scripts_menu().initialize_startup(imports_.get_command_line_a(),
                                                     bindings_.startup_scripts);
    // Lua controls must exist before the preset restores their saved values.
    bindings_.menu.preset_manager().load_startup();
    if (engine::invoke<bool>(hooks.engine, 26)) {
        reset_startup_acknowledgements(bindings_.acknowledgements);
        reset_command_state(bindings_.command_reset);
        objects = hooks.owners.entries();
        for (auto* object : objects)
            engine::invoke<void>(object, 7);
        bindings_.window.input.enable_cursor();
        request_initial_connection_refresh(state_.initial_refresh_requested,
                                           state_.full_update_pending, nullptr);
    }
    install_startup_window(bindings_.menu.window(), bindings_.window);
}
}
