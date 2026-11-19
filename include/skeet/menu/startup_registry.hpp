#pragma once
#include "skeet/config_storage/owner.hpp"

namespace skeet::ui {
// Return the saved configuration stream from HKCU\Software.
std::optional<ConfigBytes> read_startup_registry_value(const char16_t* name);

// Devirtualized4348DB32 unlinks only the ten settings descriptors and the
// separate hotkey link. Their constructed values and links remain in storage.
void unlink_startup_settings_records(SettingsPersistenceState&);
// Reciprocal of the proven433EEC8B record writer, consumed through ordinary
//43367569/433CFFB4. The present-value VM continuation434A6FCC is erased; this
// operation reconstructs its persisted-format behavior without executing it.
// Missing records preserve their constructed values and are reported false.
std::array<bool, 10>
restore_startup_settings_records(SettingsPersistenceState&,
                                 std::span<const std::byte> stream);
} //namespace skeet::ui
