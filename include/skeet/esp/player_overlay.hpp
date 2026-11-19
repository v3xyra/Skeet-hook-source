#pragma once
#include "config.hpp"
#include "flags.hpp"
#include "skeet/core.hpp"

namespace skeet::esp {

// Display up to 20 Unicode code points from the engine's UTF-8 name field.
std::u16string player_name(std::string_view utf8);

void draw_box(ui::RenderQueue& queue, Rectangle box, std::uint32_t color);
void draw_health(ui::RenderQueue& queue, Rectangle box, std::int32_t health,
                 std::int32_t number_threshold);
void draw_health(ui::RenderQueue& queue, Rectangle box, std::int32_t health,
                 std::int32_t number_threshold, std::uint32_t color);
void draw_name(ui::RenderQueue& queue, Rectangle box, std::u16string_view name, std::uint32_t color,
               std::int32_t normal_font_height);
// Requires a valid weapon, ammunition != -1, and positive capacity, matching
// the native gate. Returns the new baseline for labels below the player.
std::int32_t draw_ammo(ui::RenderQueue& queue, Rectangle box, std::int32_t baseline,
                       std::int32_t ammunition, std::int32_t capacity, float reload_progress,
                       std::uint32_t color);
std::int32_t distance_in_feet(float dx, float dy, float dz) noexcept;
void draw_distance(ui::RenderQueue& queue, std::int32_t center_x, std::int32_t baseline, float dx,
                   float dy, float dz);
struct OverlayInput {
    Rectangle box;
    std::int32_t health, health_number_threshold;
    bool player_info_available;
    std::string_view name_utf8;
    bool dormant;
    bool money_enabled;
    std::int32_t money;
    std::uint32_t flags;
    bool weapon_valid;
    std::int32_t ammunition, capacity;
    float reload_progress;
    float view_delta_x, view_delta_y, view_delta_z;
    std::int32_t entity_index;
};
struct OverlayCallbacks {
    void* context = nullptr;
    // Native 0x4336918D extension boundary, called after the 18 built-in
    // entries even when their mask is zero. The cursor is mutable.
    void (*extra_flags)(void*, ui::RenderQueue&, std::int32_t, Point&, std::int32_t) = nullptr;
};
struct OverlayLayout {
    Point side;
    std::int32_t baseline;
};
// Composes the recovered box/name/health/money/flags/ammo/distance blocks in
// original order. Input is a successfully projected, selected player. Weapon
// text/icon, skeleton, glow and offscreen gates belong to the enclosing owner.
OverlayLayout draw_player_overlay(ui::RenderQueue& queue, const OverlayConfig& config,
                                  const OverlayInput& input,
                                  std::span<const FlagEntry, 18> flag_table,
                                  OverlayCallbacks callbacks = {});

} // namespace skeet::esp
