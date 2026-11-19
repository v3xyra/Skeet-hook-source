#include "skeet/esp/player_overlay.hpp"
#include "detail.hpp"
#include "skeet/font_metrics.hpp"
#include <stdexcept>
#define NOMINMAX
#include <windows.h>

namespace skeet::esp {
using namespace detail;

std::u16string player_name(std::string_view utf8) {
    // Even 20 four-byte characters fit in this prefix. Decode before applying
    // the display limit so the last visible character is always complete.
    wchar_t output[80];
    utf8 = utf8.substr(0, std::size(output));
    utf8 = utf8.substr(0, utf8.find('\0'));
    if (utf8.empty())
        return {};
    const auto count = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()),
                                           output, static_cast<int>(std::size(output)));
    int units = 0;
    for (int characters = 0; units < count && characters < 20; ++characters) {
        if (IS_HIGH_SURROGATE(output[units]) && units + 1 < count)
            ++units;
        ++units;
    }
    return {output, output + units};
}

void draw_box(ui::RenderQueue& q, Rectangle b, std::uint32_t color) { // 0x4340AB68..0x4340AC1C
    q.rectangle_outline({sub(b.x, 1), sub(b.y, 1), add(b.width, 2), add(b.height, 2), 0x8C000000u},
                        1);
    q.rectangle_outline({b.x, b.y, b.width, b.height, color}, 1);
    q.rectangle_outline({add(b.x, 1), add(b.y, 1), sub(b.width, 2), sub(b.height, 2), 0x8C000000u},
                        1);
}

void draw_health(ui::RenderQueue& q, Rectangle b, i32 health,
                 i32 threshold) { // 0x4340A44D..0x4340A5C4
    const auto value = std::clamp(health, 0, 100);
    const auto fraction = (float(std::clamp(value, 25, 75)) - 25.0f) / 50.0f;
    const float green = fraction * 175.0f + 50.0f + 8388608.0f;
    const float red = 255.0f - fraction * 135.0f + 8388608.0f;
    const auto color = 0xFF500000u | ((std::bit_cast<std::uint32_t>(green) & 255) << 8) |
                       (std::bit_cast<std::uint32_t>(red) & 255);
    draw_health(q, b, health, threshold, color);
}

void draw_health(ui::RenderQueue& q, Rectangle b, i32 health, i32 threshold, std::uint32_t color) {
    const auto value = std::clamp(health, 0, 100);
    const auto opacity = color >> 24;
    const auto background = ((opacity * 160u) / 255u) << 24;
    const auto number_color = (((opacity * 240u) / 255u) << 24) | 0x00FFFFFFu;
    const auto filled = mul(b.height, value) / 100;
    const auto empty = sub(b.height, filled);
    q.rectangle_outline({sub(b.x, 6), sub(b.y, 1), 4, add(b.height, 2), background}, 1);
    q.rectangle({sub(b.x, 6), sub(b.y, 1), 4, empty, background});
    q.rectangle({sub(b.x, 5), add(b.y, empty), 2, filled, color});
    if (value <= threshold || health > 100)
        q.text(sub(b.x, 7), add(b.y, add(empty, 1)), number_color, 0x14, 0, decimal(health));
}

void draw_name(ui::RenderQueue& q, Rectangle b, std::u16string_view text, std::uint32_t color,
               i32 height) { // 0x4340A674..0x4340A6A6
    q.text(add(b.x, b.width / 2), sub(b.y, add(height, 2) / 2), color, 0x18, 0, text);
}

i32 draw_ammo(ui::RenderQueue& q, Rectangle b, i32 baseline, i32 count, i32 capacity, float reload,
              std::uint32_t color) { // 0x4340A8AE..0x4340A9FD
    const auto progress = reload != 1.0f ? reload : float(count) / float(capacity);
    const auto fraction = sse_min(sse_max(progress, 0.0f), 1.0f);
    const auto filled = truncate(float(b.width) * fraction);
    const auto background = (((color >> 24) * 160) / 255) << 24;
    q.rectangle({sub(b.x, 1), add(baseline, 2), add(b.width, 2), 4, background});
    q.rectangle({b.x, add(baseline, 3), filled, 2, color});
    if (fraction < 0.25f && reload == 1.0f)
        q.text(add(b.x, filled), add(baseline, 5), 0xC8FFFFFFu, 0x14, 0, decimal(count));
    return add(baseline, 5);
}

i32 distance_in_feet(float dx, float dy, float dz) noexcept { // 0x4340AA17..0x4340AAC6
    const float distance = std::sqrt((dx * dx + dy * dy) + dz * dz);
    return mul(sub(2, truncate(distance / -12.0f)) / 5, 5);
}

void draw_distance(ui::RenderQueue& q, i32 x, i32 baseline, float dx, float dy, float dz) {
    auto text = decimal(distance_in_feet(dx, dy, dz));
    text += u" FT";
    q.text(x, add(baseline, 6), 0xC8FFFFFFu, 0x14, 0, text);
}

OverlayLayout draw_player_overlay(ui::RenderQueue& q, const OverlayConfig& c, const OverlayInput& p,
                                  std::span<const FlagEntry, 18> table,
                                  OverlayCallbacks callbacks) {
    const auto b = p.box;
    OverlayLayout result{{add(add(b.x, b.width), 2), b.y}, add(b.y, b.height)};
    if (!(c.bounding_box.value || c.health_bar.value || c.name.value || c.flags.value ||
          c.ammo.value || c.distance.value))
        return result;
    if (c.health_bar.value)
        draw_health(q, b, p.health, p.health_number_threshold, c.health_color.value);
    if (!q.fonts)
        throw std::logic_error("Player ESP requires the renderer font backend");
    const auto small_height = ui::font_height(*q.fonts, 4);
    const auto normal_height = ui::font_height(*q.fonts, 0);
    if (c.name.value && p.player_info_available) {
        const auto name = player_name(p.name_utf8);
        if (!name.empty())
            draw_name(q, b, name, c.name_color.value, normal_height);
    }
    if (p.money_enabled && !p.dormant) {
        auto amount = std::u16string(u"$") + decimal(p.money);
        q.text(result.side.x, result.side.y, 0xD215C27Bu, 4, 0, amount);
        result.side.y = add(result.side.y, small_height);
    }
    if (c.flags.value) {
        draw_flags(q, result.side, small_height, p.flags, table);
        if (callbacks.extra_flags)
            callbacks.extra_flags(callbacks.context, q, p.entity_index, result.side, small_height);
    }
    if (p.weapon_valid && c.ammo.value && !p.dormant && p.ammunition != -1 && p.capacity > 0)
        result.baseline = draw_ammo(q, b, result.baseline, p.ammunition, p.capacity,
                                    p.reload_progress, c.ammo_color.value);
    if (c.distance.value) {
        draw_distance(q, add(b.x, b.width / 2), result.baseline, p.view_delta_x, p.view_delta_y,
                      p.view_delta_z);
        result.baseline = add(result.baseline, small_height);
    }
    if (c.bounding_box.value)
        draw_box(q, b, c.box_color.value);
    return result;
}

} // namespace skeet::esp
