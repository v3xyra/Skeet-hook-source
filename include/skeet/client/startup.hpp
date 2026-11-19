#pragma once
#include "skeet/client/startup_reset.hpp"
#include "skeet/engine/startup_convars.hpp"
#include <cstddef>
#include <cstdint>

namespace skeet::engine {
struct UnloadState;
}
namespace skeet::input {
class WindowSubclassRuntime;
}
namespace skeet {
class ConfigStorageOwner;
}
namespace skeet::script {
class ScriptOwner;
}
namespace skeet::ui {
class IconCache;
class RendererDeviceListener;
class RootMenu;
class MenuWindow;
struct StartupIcons;
}

namespace skeet::client {
class ShutdownRuntime;
#if defined(_M_IX86) || defined(__i386__)
using StartupFactory = void*(__cdecl*)();
using StartupCommandLine = const char*(__stdcall*)();
#else
using StartupFactory = void* (*)();
using StartupCommandLine = const char* (*)();
#endif

struct StartupImports {
    StartupCommandLine get_command_line_a;
};
StartupImports native_startup_imports();
// The startup, unload and timer finalization paths share this MenuWindow's
// resolved HWND cell; the separate subclass installation state is transient.
void install_startup_window(ui::MenuWindow&, input::WindowSubclassRuntime&);

// The optional owner43467C04 points to this native vector through its first
// field. Its count and storage are reread after each virtual0 notification.
struct StartupRefreshVector {
    void** objects;
    std::int32_t unknown_4, unknown_8, count;
};
struct StartupRefreshSource {
    StartupRefreshVector* vector;
};
#if UINTPTR_MAX == UINT32_MAX
static_assert(offsetof(StartupRefreshVector, count) == 0xc);
#endif

struct PostInitializationState {
    StartupFactory& first_factory; // 434678C4, actual resolved engine function
    void*& first_result; // 43467460
    StartupFactory& second_factory; // 43468B9C, actual resolved engine function
    void*& second_result; // 43476424
    StartupRefreshSource*& refresh_source; // 43467C04
    std::uint8_t& full_update_pending; // 43475BD2
    std::uint8_t& initial_refresh_requested; // 434758B0
};

// Live decoded views of the canonical encoded engine ConVar words.
struct StartupConVars {
    engine::NativeConVarReference at_43467fbc; // ^96BEB183
    engine::NativeConVarReference at_43467130; // ^99854835
    engine::NativeConVarReference at_4346a5b4; // ^7CBE81A1
    engine::NativeConVarReference at_43468ba8; // ^30911EAB
    engine::NativeConVarReference at_434671ac; // ^D8BFAEFF
    engine::NativeConVarReference at_4346747c; // ^03B4819C
    engine::NativeConVarReference at_4346a748; // ^7D9DC86F, saved dword+60 around float setter
    engine::NativeConVarReference at_4346a330; // ^BE64D223
};

struct PostInitializationBindings {
    engine::UnloadState& hooks;
    ui::RendererDeviceListener& renderer_listener;
    ui::RootMenu& menu;
    ui::IconCache& icons;
    ui::StartupIcons& startup_icons;
    input::WindowSubclassRuntime& window;
    CommandResetState command_reset;
    std::span<StartupAcknowledgement, 150> acknowledgements; // 43475BE0
    std::span<const std::uint32_t, 128> startup_scripts; // mainmenu+120
    ConfigStorageOwner& configuration_storage;
    script::ScriptOwner& scripts;
    ShutdownRuntime& finalizer;
};

// Native4341F57F..4341F7CD, after the recovered menu and engine bindings exist.
// This entry does not stand in for the earlier protected binding scheduler.
class PostInitializationRuntime {
public:
    PostInitializationRuntime(PostInitializationState state, StartupConVars variables,
                              PostInitializationBindings bindings, StartupImports imports);
    void configure_variables(); // 4341F652..4341F740
    void initialize(); // complete valid-code-pointer path4341F57F
private:
    PostInitializationState state_;
    StartupConVars variables_;
    PostInitializationBindings bindings_;
    StartupImports imports_;
};
}
