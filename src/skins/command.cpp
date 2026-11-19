#include "skeet/skins/command.hpp"
#include "skeet/skins/widget_setters.hpp"
#include "skeet/menu/skins_menu.hpp"
#include "skeet/menu/legit_menu.hpp"
#include "skeet/menu/rage_weapon_selector.hpp"
#include "skeet/menu/rage_menu.hpp"
#include "skeet/menu/root_menu.hpp"
#include <bit>
#include <cstring>

namespace skeet::skins {
namespace {
constexpr std::array<std::uint8_t, 65> weapon_groups{
    0, 9, 1, 1, 1, 0, 0, 3, 3, 7,  3, 6, 3, 3, 5, 0, 3, 2, 0, 2, 0, 0,
    0, 2, 2, 4, 2, 4, 5, 4, 1, 11, 1, 2, 2, 4, 1, 0, 6, 3, 8, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 3, 1, 0, 1, 10};
}
std::int32_t weapon_rage_group(std::int32_t definition, const ui::RageWeaponSelector& selector,
                               std::span<const std::uint32_t, 3> classification) noexcept {
    const auto id = static_cast<std::uint32_t>(definition);
    if (id >= weapon_groups.size() || !(classification[id >> 5] & (1u << (id & 31))))
        return -1;
    const auto group = weapon_groups[id];
    const auto* enabled = selector.enabled_groups[group];
    return enabled && *enabled ? group : 0;
}
void sync_rage_weapon_group(std::int32_t definition, ui::RageWeaponSelector& selector,
                            std::span<const std::uint32_t, 3> classification) {
    const auto group = weapon_rage_group(definition, selector, classification);
    if (group >= 0)
        selector.set_value(static_cast<std::uint32_t>(group));
}
void sync_legit_weapon_kind(std::int32_t kind, ui::LegitWeaponSelector& selector) {
    set_combo_value_atomic(selector, static_cast<std::uint32_t>(kind));
}
void CommandRuntime::phase4(const client::CommandFrame& frame, void* local_player) {
    refresh_.process_deferred_hud_refresh();
    if (!local_player)
        return;
    const auto definition = frame.weapon_id_d8;
    if (static_cast<std::int32_t>(previous_weapon) == definition)
        return;
    const auto team_offset = std::ptrdiff_t(*offsets_.pointers.at(0x434688B0) ^
                                            offsets_.keys.at(0x4346A518) ^ 0x8B5586F6u);
    std::int32_t team;
    std::memcpy(&team, static_cast<const std::byte*>(local_player) + team_offset, sizeof(team));
    previous_weapon = std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(definition));
    auto* menu = menu_;
    if (static_cast<std::uint32_t>(definition) <= 1000u) {
        menu->skins_menu().select_weapon(definition, team);
        menu = menu_;
    }
    const auto kind = std::bit_cast<std::int8_t>(frame.weapon_kind_124);
    if (static_cast<std::uint32_t>(std::int32_t(kind) - 1) <= 5u)
        sync_legit_weapon_kind(kind, menu->legit_menu().weapon_selector());
    sync_rage_weapon_group(frame.weapon_id_d8, menu_->rage_weapon_menu().selector(),
                           classification_);
}
} // namespace skeet::skins
