#pragma once
#include "misc_menu.hpp"
#include "window_events.hpp"
#include "skeet/engine/native_data_image.hpp"

namespace skeet::ui {

// Source backing for the native raw globals consumed by SettingsMenuValues.
// These are image-data defaults; constructing this storage does not run a CRT
// initializer or restore persisted settings. Keep it alive for every binding.
class SettingsMenuStorage {
    struct Defaults {
        RegisteredHotkey menu_key{};
        std::uint32_t menu_color = 0xFF2BCA9Fu;
        std::int32_t dpi_scale = 0;
        BooleanConfig multithreading_config{
            {{nullptr, nullptr, 0x4F7D43EDu}, 1, ConfigType::boolean, 0}, 0};
        std::int32_t maximum_unlag = 200, maximum_usercmd_ticks = 16, clock_correction = 30;
        std::uint8_t hold_aim = 1, fix_viewmodel_lag = 1, fix_networking = 1, fix_animations = 1,
                     faster_grenade_toss = 1;
    } defaults_;
    std::shared_ptr<engine::NativeDataImage> image_;

public:
    explicit SettingsMenuStorage(WindowEventState& = window_event_state()) noexcept;
    // Borrow the exact source cells consumed by MenuStartupData and runtime
    // owners. Binding preserves all existing values and registry links.
    explicit SettingsMenuStorage(std::shared_ptr<engine::NativeDataImage>);
    SettingsMenuStorage(const SettingsMenuStorage&) = delete;
    SettingsMenuStorage& operator=(const SettingsMenuStorage&) = delete;
    SettingsMenuStorage(SettingsMenuStorage&&) = delete;
    SettingsMenuStorage& operator=(SettingsMenuStorage&&) = delete;

    // 43478DB8 starts zero-filled. The actual 434928A2 metadata slot alone
    // calls initialize_menu_hotkey on this same object, registering Delete.
    RegisteredHotkey& menu_key;
    std::uint32_t& menu_color; //43468FB0
    std::int32_t& dpi_scale; //43475A94; GuiDpiState::selection aliases this
    std::uint8_t& lock_layout; //43467E4B; same byte as WindowEventState
    // The hidden checkbox aliases a registered descriptor's value, not a
    // second raw byte. Its actual43492921 CRT body later writes1.
    BooleanConfig& multithreading_config; //434683EC,value434683FC
    std::uint8_t& multithreading;
    std::int32_t& maximum_unlag; //43468230
    std::int32_t& maximum_usercmd_ticks; //43469274
    std::int32_t& clock_correction; //43468290
    std::uint8_t& hold_aim; //43468D7C
    std::uint8_t& fix_viewmodel_lag; //4346A328
    std::uint8_t& fix_networking; //43467CC0
    std::uint8_t& fix_animations; //4346A527
    std::uint8_t& faster_grenade_toss; //43467560

    // Menu, persistence, prediction and tick-shift bindings borrow these cells.
    // GuiDpiState must likewise be constructed from dpi_scale, not a copy.
    SettingsMenuValues values() noexcept;
};

// Original static image defaults, independently checked against skeet.bin.
// Run only on new zeroed source storage, before CRT descriptor registration.
void initialize_source_settings_menu_data(engine::NativeDataImage&);

} //namespace skeet::ui
