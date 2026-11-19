#include "skeet/menu/startup_registry.hpp"
#include <array>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

namespace skeet::ui {
namespace {
template <class T> void unlink(T& link) {
    link.previous->next = link.next;
    link.next->previous = link.previous;
}
const char* settings_header_error(std::span<const std::byte> stream) {
    if (stream.size() < 12)
        return "truncated settings header";
    std::uint32_t magic;
    std::memcpy(&magic, stream.data(), sizeof magic);
    return magic == config_stream_magic ? nullptr : "unrecognized settings signature";
}
#if defined(_WIN32)
const char* check_registry_settings(DWORD type, ConfigBytes& stream) {
    if (type != REG_SZ && type != REG_BINARY && type != REG_NONE)
        return "unexpected registry type";
    const auto error = settings_header_error(stream);
    if (error || type != REG_SZ)
        return error;

    // A string-typed value can hold a counted settings stream with embedded
    // NULs. An odd-length stream can have a pad byte before its UTF-16 terminator.
    std::span<const std::byte> records(stream);
    constexpr std::size_t max_padding = 1 + sizeof(char16_t);
    for (std::size_t padding = 0; padding <= max_padding; ++padding) {
        if (valid_config_stream(records)) {
            stream.resize(records.size());
            return nullptr;
        }
        if (padding == max_padding || records.back() != std::byte{})
            break;
        records = records.first(records.size() - 1);
    }
    return "invalid settings record length";
}
#endif
}
std::optional<ConfigBytes> read_startup_registry_value(const char16_t* name) {
    if (!name || !*name)
        throw std::invalid_argument("Missing startup settings value name");
#if defined(_WIN32)
    static_assert(sizeof(wchar_t) == sizeof(char16_t));
    const std::u16string_view value_name(name);
    const std::string path = "HKCU\\Software\\" + std::string(value_name.begin(), value_name.end());
    const auto exists = [&](LSTATUS status) {
        if (status == ERROR_FILE_NOT_FOUND || status == ERROR_PATH_NOT_FOUND)
            return false;
        if (status != ERROR_SUCCESS)
            throw std::runtime_error(path + " (Windows error " + std::to_string(status) + ")");
        return true;
    };
    HKEY raw_key = nullptr;
    if (!exists(RegOpenKeyExW(HKEY_CURRENT_USER, L"Software", 0, KEY_QUERY_VALUE, &raw_key)))
        return std::nullopt;
    const std::unique_ptr<std::remove_pointer_t<HKEY>, decltype(&RegCloseKey)> key(raw_key, RegCloseKey);
    const auto* value = reinterpret_cast<const wchar_t*>(name);
    for (;;) {
        DWORD size = 0, type = REG_NONE;
        if (!exists(RegQueryValueExW(key.get(), value, nullptr, &type, nullptr, &size)))
            return std::nullopt;
        ConfigBytes stream(size);
        if (size) {
            // Supply the stored byte count exactly. String termination must not
            // overwrite the final byte of an odd-length binary settings record.
            const auto status = RegQueryValueExW(key.get(), value, nullptr, &type,
                                                 reinterpret_cast<BYTE*>(stream.data()), &size);
            if (status == ERROR_MORE_DATA)
                continue;
            if (!exists(status))
                return std::nullopt;
            if (size != stream.size())
                continue;
        }
        if (const auto error = check_registry_settings(type, stream))
            throw std::runtime_error(path + " (type " + std::to_string(type) + ", " +
                                     std::to_string(size) + " bytes): " + error);
        return stream;
    }
#else
    throw std::runtime_error("Startup registry input requires Windows");
#endif
}
void unlink_startup_settings_records(SettingsPersistenceState& state) {
    auto* const owner = const_cast<std::byte*>(state.native_owner());
    if (!owner)
        throw std::invalid_argument(
            "Startup settings unlink requires its constructed native owner");
    constexpr std::array<std::size_t, 10> offsets{0,    0x14, 0x3C,  0x50,  0x64,
                                                  0x78, 0x8C, 0x29C, 0x2B0, 0x314};
    unlink(*reinterpret_cast<ConfigLink*>(owner));
    unlink(*reinterpret_cast<ConfigLink*>(owner + 0x14));
    unlink(*reinterpret_cast<HotkeyLink*>(owner + 0x24));
    for (std::size_t index = 2; index < offsets.size(); ++index)
        unlink(*reinterpret_cast<ConfigLink*>(owner + offsets[index]));
}
std::array<bool, 10> restore_startup_settings_records(SettingsPersistenceState& state,
                                                      std::span<const std::byte> stream) {
    if (const auto error = settings_header_error(stream))
        throw std::invalid_argument(error);
    state.startup_preset_hash = 0;
    if (const auto preset = find_config_record(stream, startup_preset_record))
        std::memcpy(&state.startup_preset_hash, preset->value.data(), sizeof(state.startup_preset_hash));
    auto* const owner = const_cast<std::byte*>(state.native_owner());
    if (!owner)
        throw std::invalid_argument(
            "Startup settings restore requires its constructed native owner");
    constexpr std::array<std::size_t, 10> offsets{0,    0x14, 0x3C,  0x50,  0x64,
                                                  0x78, 0x8C, 0x29C, 0x2B0, 0x314};
    std::array<bool, 10> records{};
    for (std::size_t index = 0; index < offsets.size(); ++index) {
        records[index] = find_config_record(stream, state.record_keys[index]).has_value();
        if (records[index])
            (void)deserialize_config_value(
                *reinterpret_cast<ConfigDescriptor*>(owner + offsets[index]), stream, false);
    }
    return records;
}
} //namespace skeet::ui
