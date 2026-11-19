#include "skeet/client/source_application.hpp"
#include "skeet/client/runtime_storage.hpp"
#include "skeet/client/runtime_composition.hpp"
#include "skeet/client/player_runtime_owner.hpp"
#include "skeet/client/crt_startup_bindings.hpp"
#include "skeet/client/native_startup.hpp"
#include "skeet/client/source_feature_runtime.hpp"
#include "skeet/client/source_startup_cleanup.hpp"
#include "skeet/client/shutdown_runtime_owner.hpp"
#include "skeet/client/startup_bindings.hpp"
#include "skeet/client/final_menu_startup.hpp"
#include "skeet/client/final_patch_startup.hpp"
#include "skeet/animation/fix_animation_backup_cleanup.hpp"
#include "skeet/script/source_runtime_owner.hpp"
#include "skeet/script/source_executable_image.hpp"
#include "skeet/script/required_module_storage.hpp"
#include "skeet/render/renderer_assets.hpp"
#include "skeet/render/startup_renderer.hpp"
#include "skeet/menu/startup_window.hpp"
#include "skeet/menu/startup_config_path.hpp"
#include "skeet/misc/settings_runtime_owner.hpp"
#include "skeet/engine/build_hook_initialization.hpp"
#include "skeet/engine/hook_runtime_owner.hpp"
#include "skeet/engine/build_material_bindings.hpp"
#include "skeet/engine/build_skin_bindings.hpp"
#include "skeet/network/startup_user_messages.hpp"
#include <intrin.h>
#include <optional>
#include <stdexcept>
#include <string>

namespace skeet::client {
struct SourceApplication::Data {
    void* module;
    SourceDllShutdownCompletion completion;
    ClientRuntimeStorage storage;
    CrtRuntimeOwner crt;
    ConfigurationRuntimeOwner configuration;
    WindowRuntimeOwner window;
    PlayerRuntimeOwner players;
    script::RequiredModuleStorage script_modules;
    script::SourceScriptRuntimeOwner scripts;
    ui::RendererAssets assets;
    StaticCrtRuntimeOwner static_crt;
    animation::FixAnimationBackupCleanupContext animation_cleanup;
    NativeStartupSources sources;
    NativeStartupRuntime startup;
    std::optional<EngineRuntimeBindings> engine;
    std::unique_ptr<ui::StartupRenderer> renderer;
    std::unique_ptr<ui::MainMenuWindowOwner> main_window;
    std::unique_ptr<ConfigStorageOwner> presets;
    std::unique_ptr<ClientShutdownOwner> shutdown;
    std::unique_ptr<script::SourceScriptExecutableImage> executable_image;
    std::unique_ptr<SourceFeatureRuntime> features;
    std::unique_ptr<FinalMenuStartupTail> menu_tail;
    std::unique_ptr<PostInitializationOwner> post;
    bool started = false, complete = false, cleanup_started = false, cleanup_complete = false;
    bool constructing_features = false;
    bool focus_publication_started = false;
    const char* stage = "source CRT initialization";

    NativeInitializerCall final_initializer() noexcept {
        return {0x43484F06u, this, initialize_final};
    }
    static void initialize_final(void* context) {
        static_cast<Data*>(context)->initialize_engine_and_features();
    }

    Data(void* source_module, SourceDllShutdownCompletion final_completion)
        : module(source_module), completion(final_completion), crt(storage), configuration(storage),
          window(storage), players(storage),
          scripts(storage, window.unload_state(), crt.ui_types(), script_modules.builtins(),
                  script_modules.modules()),
          assets(storage),
          // The native inline containers retained unconsumed incoming ECX in
          // these two words. The source containers initialize unused storage
          // deterministically; neither word supplies an engine or feature input.
          static_crt(crt, window, assets, {0, 0}), animation_cleanup(storage),
          sources(storage.data(), final_initializer()),
          startup(sources, native_startup_runtime_imports()) {
        if (!module)
            throw std::invalid_argument("Source application requires its actual DLL module");
        script_modules.encrypt_for(scripts.runtime());
        bind_early_crt_sources(sources, crt, configuration, static_crt, scripts, animation_cleanup,
                               scripts.source_terminal_cleanup(players));
    }

    void initialize_engine_and_features() {
        stage = "engine interfaces, offsets and module bindings";
        const auto& imports = window.startup_imports();
        engine.emplace(
            storage.initialize_engine_bindings(imports, static_cast<std::uint32_t>(__rdtsc())));
        stage = "renderer and main window";
        renderer = std::make_unique<ui::StartupRenderer>(
            storage.data_owner(), storage.menu_startup_data().dpi, assets.device_listener());
        assets.bind_renderer(*renderer);
        main_window = std::make_unique<ui::MainMenuWindowOwner>(storage);
        presets = std::make_unique<ConfigStorageOwner>(
            storage.configuration(), storage.config_storage_owner(), main_window->window(),
            scripts.bridge(), scripts.ui_bindings(), storage.menu_startup_data().settings_values(),
            storage.menu_startup_data().script_values());
        shutdown = std::make_unique<ClientShutdownOwner>(storage, window, *main_window, *presets,
                                                         scripts, *renderer);
        shutdown->select_source_dll(module, completion);
        stage = "engine hook descriptors";
        engine::prepare_build_hook_descriptors(crt.descriptor_storage(), storage.memory(),
                                               imports.images, crt.hook_system());
        stage = "Lua image, material and skin bindings";
        executable_image =
            std::make_unique<script::SourceScriptExecutableImage>(storage.data_owner());
        const auto materials = engine::initialize_build_material_bindings(
            storage.data(), imports.modules, imports.images);
        const auto skin_engine =
            engine::initialize_build_skin_bindings(storage.data(), imports.images);
        stage = "menu cursor and input interfaces";
        storage.bind_input_interfaces();
        stage = "source feature contexts";
        features = std::make_unique<SourceFeatureRuntime>(
            SourceFeatureInputs{storage, *engine, crt, configuration, static_crt, window, players,
                                scripts, animation_cleanup, assets, *renderer, *main_window,
                                *presets, *shutdown, materials, skin_engine, final_initializer()});
        stage = "57 feature factories";
        constructing_features = true;
        features->initialize_features(sources);
        constructing_features = false;
        stage = "menu and runtime hook contexts";
        (void)ui::initialize_main_menu_config_path(storage.config_storage());
        auto& menu = features->compose_menu();
        features->bind_hook_contexts();
        features->menu_settings().capture_default_layout(menu);
        stage = "saved menu settings";
        menu_tail = std::make_unique<FinalMenuStartupTail>(
            storage, *main_window, features->menu_settings(), *presets, *renderer);
        menu_tail->initialize();
        stage = "post initialization contexts and feature callbacks";
        post = std::make_unique<PostInitializationOwner>(
            storage, *engine, *main_window, *renderer, assets.icon_cache(), assets.startup_icons(),
            window.subclass(), *presets, scripts.require_owner(), shutdown->finalizer());
        window.bind_subclass_callback();
        initialize_final_feature_callbacks(storage);
        stage = "client class and receive proxy patches";
        capture_final_startup_patches(storage, features->player_receives(),
                                      features->visual_receives(), features->skins(),
                                      features->class_factories());
        stage = "SDK user message bindings";
        network::prepare_startup_user_message_patches(storage.data());
        stage = "final engine and Steam overlay bindings";
        finish_final_engine_startup(storage, features->hooks());
        // Source concurrency policy: publish the focus callback last. Its
        // first engine invocation runs4341F57F's2/7 passes, startup scripts
        // and the window subclass. Those operations must not race the final
        // primary3 pass or expose a menu while final patch capture can fail.
        // The original initializer installed focus earlier; no final-tail
        // operation depends on that receiver already being hooked.
        stage = "focus overlay publication";
        focus_publication_started = true;
        features->hooks().initialize_focus(*post, features->visual());
        complete = true;
    }
};

SourceApplication::SourceApplication(void* module, SourceDllShutdownCompletion completion)
    : data_(std::make_unique<Data>(module, completion)) {}
SourceApplication::~SourceApplication() = default;
void SourceApplication::initialize(std::uintptr_t argument) {
    if (data_->started)
        throw std::logic_error("Source application initialization has already started");
    data_->started = true;
    try {
        (void)data_->startup.initialize(argument);
    } catch (const std::exception& error) {
        throw std::runtime_error(std::string(data_->stage) + ": " + error.what());
    }
    if (!data_->complete)
        throw std::logic_error("Source final initialization did not complete");
}
bool SourceApplication::initialized() const noexcept {
    return data_->complete;
}
bool SourceApplication::cancel_initialization() noexcept {
    auto& data = *data_;
    if (data.complete)
        return false;
    if (data.cleanup_started)
        return data.cleanup_complete;
    data.cleanup_started = true;
    try {
        if (data.shutdown) {
            data.shutdown->cancel_deferred_request();
            data.shutdown->finalizer().cancel_timer();
            // A factory can publish its vtable before a later field's C++
            // constructor throws. Executable callback addresses alone cannot
            // certify that receiver's destructor is safe to dispatch.
            if (data.constructing_features || data.focus_publication_started)
                return false;
            if (!source_startup_detach_ready(data.storage, data.window))
                return false;
            data.shutdown->finalizer().detach();
        } else {
            if (!source_exit_callbacks_ready(data.storage))
                return false;
            data.storage.exit_callbacks().run();
        }
        data.cleanup_complete = true;
    } catch (...) {
        // The registry can contain already-destroyed owners after a throwing
        // callback. Keeping the owning application resident prevents a second
        // cleanup attempt and dangling engine callbacks into an unloaded DLL.
    }
    return data.cleanup_complete;
}
ClientRuntimeStorage& SourceApplication::storage() noexcept {
    return data_->storage;
}
} //namespace skeet::client
