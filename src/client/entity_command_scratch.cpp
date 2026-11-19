#include "entity_command_scratch.hpp"
#include "skeet/client/startup_reset.hpp"
#include "skeet/engine/interface_call.hpp"
#include "skeet/engine/netvars.hpp"
#include "skeet/engine/decoded_offset.hpp"
#include "skeet/misc/native_text.hpp"
#include <array>
#include <bit>
#include <cstring>
#include <stdexcept>

namespace skeet::client::entity_command_detail {
namespace {
template <class T> T read(const void* object, std::ptrdiff_t offset = 0) noexcept {
    T value;
    std::memcpy(&value, static_cast<const std::byte*>(object) + offset, sizeof value);
    return value;
}
template <class T> void write(void* object, std::ptrdiff_t offset, const T& value) noexcept {
    std::memcpy(static_cast<std::byte*>(object) + offset, &value, sizeof value);
}
void purge_memory(void* memory, const engine::NativeDataImage& image) { //4331E2CD
    if (read<std::int32_t>(memory, 8) < 0)
        return;
    if (auto* allocation = read<void*>(memory)) {
        engine::invoke<void>(image.native_cell<void*>(0x43467204u), 5, allocation);
        write(memory, 0, static_cast<void*>(nullptr));
    }
    write(memory, 4, std::uint32_t(0));
}
void purge_vector(void* vector, const engine::NativeDataImage& image) { //433CF41B
    write(vector, 0xC, std::uint32_t(0));
    purge_memory(vector, image);
    write(vector, 0x10, read<void*>(vector));
    purge_memory(vector, image);
}
void release_record(void* record, const engine::NativeDataImage& image) { //4336B845
    auto* block = static_cast<std::byte*>(record) + 0x60;
    for (unsigned count = 0; count < 2; ++count) {
        block -= 0x30;
        purge_vector(block + 0x1C, image);
        purge_vector(block, image);
    }
}
void release_group(std::uint32_t address, const engine::NativeDataImage& image) {
    auto*& destination = image.native_cell<void*>(address);
    if (destination) {
        const auto& backup = image.native_cell<std::array<std::byte, 0x18>>(address + 4);
        std::memcpy(destination, backup.data(), backup.size());
    }
    auto*& record = image.native_cell<void*>(address + 0x30);
    if (record) {
        release_record(record, image);
        auto* pool = image.native_cell<void*>(0x4346D658u);
        if (record) {
            auto* previous = read<void*>(pool, 0x18);
            write(pool, 0xC, std::uint32_t(read<std::uint32_t>(pool, 0xC) - 1u));
            write(record, 0, previous);
            write(pool, 0x18, record);
        }
        record = nullptr;
    }
    auto*& allocation = image.native_cell<void*>(address + 0x1C);
    if (allocation) {
        engine::invoke<void>(image.native_cell<void*>(0x43467204u), 5, allocation);
        allocation = nullptr;
    }
    image.native_cell<std::array<std::byte, 0x18>>(address + 0x1C).fill(std::byte{});
}
struct AdditionalField {
    std::uint32_t offset;
    engine::DataMapField field;
};
AdditionalField additional_field(const engine::NativeOffsetGlobals& globals, std::uint32_t pointer,
                                 std::uint32_t key, std::uint32_t salt, const char* name,
                                 std::int32_t type, std::uint32_t bytes, std::uint32_t tolerance) {
    AdditionalField result{static_cast<std::uint32_t>(std::ptrdiff_t(
                               engine::DecodedOffset(globals, pointer, key, salt, 0))),
                           {}};
    result.field.type = type;
    result.field.name = name;
    write(&result.field, 0xc, std::uint16_t(1));
    write(&result.field, 0xe, std::uint16_t(0x100));
    write(&result.field, 0x20, bytes);
    write(&result.field, 0x2c, tolerance);
    return result;
}
engine::DataMap* data_map(const engine::NativeDataImage& image, std::uint32_t vtable_cell) {
    auto** table = image.native_cell<void**>(vtable_cell);
    const auto* getter = static_cast<const std::byte*>(table[17]);
    // Native inspects this six-byte constant-return getter; it never calls it.
    if (!getter || read<std::uint8_t>(getter) != 0xb8 || read<std::uint8_t>(getter, 5) != 0xc3)
        return nullptr;
    return read<engine::DataMap*>(getter, 1);
}
void rebuild_map(engine::DataMap* map, const engine::NativeDataImage& image) {
    image.native_cell<engine::MemberFunction<void>>(0x4346D65Cu)(map);
}
void append_fields(engine::DataMap* map, std::uint32_t destination,
                   std::span<const AdditionalField> extra, const engine::NativeDataImage& image) {
    if (!image.native_cell<void*>(0x4346D658u) || !image.native_cell<void*>(0x4346D65Cu) || !map)
        return;
    image.native_cell<engine::DataMap*>(destination) = map;
    auto& backup = image.native_cell<std::array<std::byte, 0x18>>(destination + 4);
    std::memcpy(backup.data(), map, backup.size());
    const auto original_count = std::bit_cast<std::uint32_t>(map->field_count);
    const auto new_count = original_count + static_cast<std::uint32_t>(extra.size());
    std::uint32_t names_offset = new_count * 0x3cu;
    auto bytes = names_offset;
    for (const auto& field : extra)
        if (field.field.name)
            bytes += static_cast<std::uint32_t>(std::strlen(field.field.name) + 1u);
    auto* allocation = static_cast<std::byte*>(
        engine::invoke<void*>(image.native_cell<void*>(0x43467204u), 1, bytes));
    if (!allocation)
        return;
    std::memcpy(allocation, map->fields, original_count * 0x3cu);
    auto* appended = allocation + std::bit_cast<std::uint32_t>(map->field_count) * 0x3cu;
    for (const auto& field : extra) {
        std::memcpy(appended, &field.field, 0x3c);
        write(appended, 8, field.offset);
        if (field.field.name) {
            const auto length = static_cast<std::uint32_t>(std::strlen(field.field.name) + 1u);
            auto* const name = reinterpret_cast<char*>(allocation + names_offset);
            std::memcpy(name, field.field.name, length);
            names_offset += length;
            write(appended, 4, name);
        }
        appended += 0x3c;
    }
    auto& clone = image.native_cell<engine::DataMap>(destination + 0x1c);
    std::memcpy(&clone, map, 0x18);
    clone.optimized = nullptr;
    clone.fields = reinterpret_cast<engine::DataMapField*>(allocation);
    clone.field_count = std::bit_cast<std::int32_t>(new_count);
    rebuild_map(&clone, image);
    if (clone.optimized)
        std::memcpy(map, &clone, 0x18);
}
// Complete43417ED1 first-match traversal. Only the matching descriptor is
// needed here; native's accompanying accumulated offset is not consumed.
engine::DataMapField* find_field(engine::DataMap* map, std::uint32_t hash) {
    for (; map; map = map->base) {
        for (std::int32_t index = 0; index < map->field_count; ++index) {
            auto& field = map->fields[index];
            if (field.name && engine::netvar_name_hash(field.name) == hash)
                return &field;
            if (field.type == 10)
                if (auto* found = find_field(field.embedded, hash))
                    return found;
        }
    }
    return nullptr;
}
void update_weapon_fields(engine::DataMap* map, const engine::NativeDataImage& image) {
    if (!map)
        return;
    bool changed = false;
    for (const auto hash : {0x1860C50Du, 0x2489A29Du}) {
        auto* field = find_field(map, hash);
        if (!field)
            continue;
        auto flags = read<std::uint16_t>(field, 0xe);
        if (flags & 0x100u)
            continue;
        flags = std::uint16_t((flags | 0x100u) & ~0x400u);
        write(field, 0xe, flags);
        write(field, 0x2c, std::uint32_t(0x3a83126f));
        changed = true;
    }
    if (!changed || !map->optimized)
        return;
    auto* const previous_record = map->optimized;
    map->optimized = nullptr;
    rebuild_map(map, image);
    auto* const pool = image.native_cell<void*>(0x4346D658u);
    auto* const previous_free = read<void*>(pool, 0x18);
    write(pool, 0xc, std::uint32_t(read<std::uint32_t>(pool, 0xc) - 1u));
    write(previous_record, 0, previous_free);
    write(pool, 0x18, previous_record);
}
}
void release_prediction_scratch(EntityCommandOwner* const& active,
                                const engine::NativeDataImage& image) {
    if (!active)
        return;
    if (image.native_cell<void*>(0x4346D658u) && image.native_cell<std::uint32_t>(0x4346D65Cu))
        release_group(0x4347BD90u, image);
    // The first group calls actual allocator methods. Native reloads all
    // three gate inputs before considering the independent second group.
    if (active && image.native_cell<void*>(0x4346D658u) &&
        image.native_cell<std::uint32_t>(0x4346D65Cu))
        release_group(0x4347BD5Cu, image);
}
void initialize_prediction_scratch(EntityCommandOwner* const& active,
                                   const engine::NativeDataImage& image,
                                   const engine::NativeOffsetGlobals& offsets,
                                   bool local_creation) {
    // Devirtualized433E33D3. The owner and both guard words are consumed before
    // either branch, including the one-shot connection notification path.
    auto* const owner = active;
    if (!owner || image.native_cell<void*>(0x4347BDACu) || image.native_cell<void*>(0x4347BD78u))
        return;
    if (!local_creation) {
        auto& checked = image.native_cell<std::uint8_t>(0x434758B0u);
        if (!checked && engine::invoke<bool>(image.native_cell<void*>(0x4346A514u), 27))
            request_initial_connection_refresh(checked, image.native_cell<std::uint8_t>(0x43475BD2u),
                                               image.native_cell<void (*)()>(0x43468BF0u));
        return;
    }
    if (owner->prediction_names.size() != 8)
        throw std::logic_error(
            "Prediction scratch requires its eight constructed entity-owner strings");
    const auto name = [&](std::size_t index) { return owner->prediction_names[index].begin; };
    const std::array player{
        additional_field(offsets, 0x4346791c, 0x43467bd4, 0x7f5ed2d0, name(0), 1, 4, 0x3c23d70a),
        additional_field(offsets, 0x43468354, 0x4346852c, 0x66146802, name(1), 1, 4, 0x3a83126f),
        additional_field(offsets, 0x4346799c, 0x4346a2ec, 0xbcb263a5, name(2), 6, 1, 0),
        additional_field(offsets, 0x434679cc, 0x43467164, 0x00a5bd68, name(3), 6, 1, 0),
        additional_field(offsets, 0x43468ee4, 0x4346853c, 0x52899df3, name(6), 5, 4, 0)};
    const std::array grenade{
        additional_field(offsets, 0x43467aa4, 0x43467bf4, 0x3b93667b, name(4), 1, 4, 0),
        additional_field(offsets, 0x43467a6c, 0x4346a658, 0xe865a877, name(5), 6, 1, 0)};
    append_fields(data_map(image, 0x43467D54u), 0x4347BD90u, player, image);
    append_fields(data_map(image, 0x4346A43Cu), 0x4347BD5Cu, grenade, image);
    update_weapon_fields(data_map(image, 0x43467F84u), image);
    write(image.native_cell<void*>(0x43469264u), 0x8c, std::uint8_t(1));
    write(image.native_cell<void*>(0x43469264u), 0x9c, std::uint32_t(0));
}
} // namespace skeet::client::entity_command_detail
