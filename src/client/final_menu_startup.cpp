#include "skeet/client/final_menu_startup.hpp"
#include "skeet/client/runtime_storage.hpp"
#include "skeet/menu/startup_window.hpp"
#include "skeet/menu/root_menu.hpp"
#include "skeet/misc/settings_runtime_owner.hpp"
#include "skeet/render/startup_renderer.hpp"
#include "skeet/render/backend.hpp"
#include "skeet/engine/owner_allocation.hpp"
#include "skeet/hotkey_widget.hpp"
#include <algorithm>
#include <bit>
#include <cstring>
#include <new>

namespace skeet::client {
namespace {
void refresh_menu_key(ui::MenuWindow& window, RegisteredHotkey& binding) {
    ui::HotkeyWidget* found = nullptr;
    for (auto* tab : window.tabs)
        for (auto* category : tab->categories)
            for (auto* widget : category->elements)
                if (auto* key = dynamic_cast<ui::HotkeyWidget*>(widget);
                    key && key->value == &binding) {
                    if (found)
                        throw std::logic_error(
                            "Startup menu key has more than one canonical widget");
                    found = key;
                }
    if (!found)
        throw std::logic_error("Startup menu key widget has not been composed");
    found->config_loaded(); //actual433ADDF2, no settings-change notification
}
}
namespace {
struct RestoredSettings {
    std::array<bool, 10> records{};
};
void restore_saved_layout(ui::RootMenu& menu, const std::array<std::int32_t, 21>& values) {
    if (values[0] != 9)
        return;
    const auto targets = menu.layout_targets();
    constexpr std::array<std::size_t, 15> indices{3,  5,  4,  6,  7,  8,  9, 10,
                                                  11, 12, 13, 14, 15, 19, 20};
    for (std::size_t index = 0; index < indices.size(); ++index) {
        const auto packed = std::bit_cast<std::uint32_t>(values[indices[index]]);
        auto& category = *targets.categories[index + 2];
        category.column = std::bit_cast<std::int8_t>(std::uint8_t(packed));
        category.column_span = std::bit_cast<std::int8_t>(std::uint8_t(packed >> 8));
        category.row = std::bit_cast<std::int8_t>(std::uint8_t(packed >> 16));
        category.row_span = std::bit_cast<std::int8_t>(std::uint8_t(packed >> 24));
    }
    // Same ordinary43342FF1 category layout/reflow/resize consumer. The two
    // player-only built-in defaults are not persisted by433EEC8B.
    for (auto* tab : menu.window().tabs) {
        tab->layout_categories();
        tab->reflow_categories();
        tab->resize(menu.window().input.slider_metrics);
    }
}
}
void capture_startup_configuration_baseline(ClientRuntimeStorage& storage) {
    auto& cells = storage.config_storage().baseline_cells();
    if (cells.begin || cells.end || cells.capacity)
        throw std::logic_error("Startup configuration baseline has already been captured");
    ConfigBytes bytes;
    if (!serialize_config_registry(storage.configuration().descriptors, bytes))
        throw std::runtime_error(
            "Startup registry contains an unsupported configuration descriptor");
    auto* const allocation =
        static_cast<std::byte*>(engine::allocate_owner_storage(storage.memory(), bytes.size()));
    if (!allocation)
        throw std::bad_alloc();
    std::memcpy(allocation, bytes.data(), bytes.size());
    const auto begin = std::uint32_t(reinterpret_cast<std::uintptr_t>(allocation));
    cells = {begin, begin + std::uint32_t(bytes.size()), begin + std::uint32_t(bytes.size())};
}
FinalMenuStartupTail::FinalMenuStartupTail(ClientRuntimeStorage& storage,
                                           ui::MainMenuWindowOwner& window,
                                           misc::SettingsRuntimeOwner& settings,
                                           ConfigStorageOwner& configuration,
                                           ui::StartupRenderer& renderer)
    : storage_(storage), window_(window), settings_(settings), configuration_(configuration),
      renderer_(renderer) {
    if (&window.menu_data().data() != &storage.data() || &settings.storage() != &storage ||
        &settings.window() != &window || &settings.configuration() != &configuration ||
        &configuration.native_storage() != &storage.config_storage() ||
        &configuration.configuration() != &storage.configuration() ||
        &renderer.dpi() != &storage.menu_startup_data().dpi)
        throw std::invalid_argument("Final menu startup requires the canonical source owners");
}
void FinalMenuStartupTail::initialize() {
    if (started_)
        throw std::logic_error("Final menu startup has already started");
    auto& menu = window_.controller();
    if (!settings_.default_layout_captured() || !configuration_.menu_bound_to(menu) ||
        storage_.data().native_cell<ui::RootMenu*>(0x43476924u) != &menu)
        throw std::logic_error(
            "Final menu startup requires the actual RootMenu and captured default layout");
    started_ = true;
    auto& persistent = storage_.config_storage().initialize_settings(storage_.memory());
    const auto value = ui::read_startup_registry_value(window_.registry_name().text.data());
    ui::unlink_startup_settings_records(persistent);
    RestoredSettings restored;
    if (value)
        restored.records = ui::restore_startup_settings_records(persistent, *value);
    auto values = storage_.menu_startup_data().settings_values();
    if (restored.records[0])
        values.menu_color = std::bit_cast<std::uint32_t>(persistent.global_value);
    if (restored.records[1]) {
        auto& binding = values.menu_key.binding;
        binding.packed = (binding.packed & 3u) | (persistent.menu_binding.packed & ~3u);
    }
    if (restored.records[2])
        values.lock_layout = std::uint8_t(persistent.input_mode);
    if (restored.records[3])
        values.dpi_scale = persistent.dpi_scale;
    if (restored.records[6])
        std::copy(persistent.script_hashes.begin(), persistent.script_hashes.end(),
                  window_.startup_scripts().begin());
    if (restored.records[9])
        storage_.menu_startup_data().script_values().allow_unsafe = persistent.flag;
    refresh_menu_key(window_.window(), storage_.menu_hotkey());
    // The window's first DPI pass precedes tab construction. Initialize the
    // completed control tree even when the saved scale is still 100%.
    window_.menu_data().dpi.refresh(true);
    auto& window = window_.window();
    if (restored.records[4] && restored.records[5])
        window.restore_size({persistent.menu_width, persistent.menu_height}, {});
    if (restored.records[8])
        restore_saved_layout(menu, persistent.menu_controls);
    settings_.runtime().previous_dpi_scale = values.dpi_scale;
    settings_.runtime().previous_lock_layout = values.lock_layout;
    capture_startup_configuration_baseline(storage_);
    // Startup can run while the game is minimized or changing video mode.
    // Place the menu on the HUD thread once its actual client area is known.
    window.startup_placement_pending = true;
    menu.initialize_renderer_resources(renderer_.renderer());
    window.resize_finished = {&configuration_.persistence(), [](void* context) {
                                  static_cast<SettingsPersistence*>(context)->persist();
                              }};
    complete_ = true;
}
} //namespace skeet::client
