#include "skeet/network/tick_shift_message.hpp"
#include "skeet/client/commands.hpp"
#include "skeet/engine/interface_call.hpp"
#include <array>
#include <bit>
#include <cstring>

namespace skeet::network {
namespace {
template <class T> T field(const void* object, std::ptrdiff_t offset) noexcept {
    T value;
    std::memcpy(&value, static_cast<const std::byte*>(object) + offset, sizeof value);
    return value;
}
template <class T> void write(void* object, std::ptrdiff_t offset, T value) noexcept {
    std::memcpy(static_cast<std::byte*>(object) + offset, &value, sizeof value);
}
std::int32_t add(std::int32_t a, std::int32_t b) noexcept {
    return std::bit_cast<std::int32_t>(std::uint32_t(a) + std::uint32_t(b));
}
std::int32_t subtract(std::int32_t a, std::int32_t b) noexcept {
    return std::bit_cast<std::int32_t>(std::uint32_t(a) - std::uint32_t(b));
}
struct MessageBytes {
    union {
        std::array<std::byte, 16> local;
        std::byte* pointer;
    } storage;
    std::uint32_t size, capacity;
};
static_assert(sizeof(MessageBytes) == 0x18);
#if defined(_M_IX86) || defined(__i386__)
void __fastcall message_entry(void* object, misc::NetworkMessageCall* call) {
#else
void message_entry(void* object, misc::NetworkMessageCall* call) {
#endif
    auto& endpoint = *static_cast<TickShiftMessageHandler*>(object);
    rewrite_tick_shift_command_message(endpoint.state, endpoint.bindings, *call);
}
const std::array<misc::NetworkMessageEntry, 1> message_table{message_entry};
}
TickShiftMessageHandler::TickShiftMessageHandler(TickShiftState& state_value,
                                             const TickShiftMessageBindings& current) noexcept
    : NetworkMessageCallback{nullptr}, state(state_value), bindings(current) {}
void TickShiftMessageHandler::publish_table() noexcept {
    vtable = message_table.data();
}

void rewrite_tick_shift_command_message(const TickShiftState& state,
                                     const TickShiftMessageBindings& bindings,
                                     misc::NetworkMessageCall& call) {
    if (engine::invoke<std::int32_t>(call.message, 7) != 9)
        return;
    void* const message = call.message;
    if ((field<std::uint32_t>(message, 0x1C) & 6U) != 6U)
        return;
    const auto saved_count = field<std::int32_t>(message, 0x10);
    if (saved_count == 0 || !(field<std::uint8_t>(message, 0x1C) & 1U) ||
        field<std::int32_t>(message, 0xC) != 2)
        return;
    const auto choked = *bindings.choked;
    if (choked == 0 && saved_count == 1) {
        const auto next = add(*bindings.last_outgoing, 1);
        const auto* ring = field<const client::Command*>(bindings.input, 0xF0);
        const auto& command = ring[std::uint32_t(next) % 150U];
        if (command.number == next && std::bit_cast<std::uint32_t>(command.tick) == 0x7F7FFFFFU) {
            call.cancelled = 1;
            return;
        }
    }
    if (state.budget <= 15)
        return;
    auto requested = add(choked, 1);
    if (requested < 0)
        requested = 0;
    if (subtract(requested, 15) <= 0)
        return;

    const auto saved_data = field<void*>(message, 0x14);
    void* const channel = call.channel;
    MessageBytes encoded;
    encoded.storage.local.fill(std::byte{0});
    encoded.size = 0;
    encoded.capacity = 15;
    std::array<std::byte, 4001> bytes;
    TickShiftCommandWriter writer{bytes.data(), 4000, 32000, 0, 0, 0, 0, nullptr};
    write(message, 0xC, std::int32_t(2));
    write(message, 0x14, &encoded);
    const auto count = requested < 60 ? requested : 60;
    write(message, 0x10, count);
    const auto last = add(add(*bindings.choked, 1), *bindings.last_outgoing);
    auto to = add(subtract(last, count), -1);
    std::int32_t from = -1;
    bool complete = true;
    while (to <= last) {
        const auto first_new = add(subtract(last, field<std::int32_t>(message, 0x10)), 1);
        if (!engine::invoke<std::uint8_t>(bindings.client, 24, std::int32_t(0), &writer, from, to,
                                          std::uint8_t(to >= first_new))) {
            complete = false;
            break;
        }
        from = to;
        to = add(to, 1);
    }
    if (complete) {
        encoded.size = std::uint32_t(add(writer.current_bit, 7) >> 3);
        if (encoded.size < 16)
            std::memmove(encoded.storage.local.data(), bytes.data(), encoded.size);
        else {
            encoded.storage.pointer = bytes.data();
            encoded.capacity = 4000;
            bytes[encoded.size] = std::byte{0};
        }
        using Send = engine::MemberFunction<std::uint8_t, void*, std::uint8_t, std::uint8_t>;
        reinterpret_cast<Send>(bindings.original_channel_vtable[40])(
            channel, message, std::uint8_t(0), std::uint8_t(0));
    }
    call.cancelled = 1;
    write(message, 0x14, saved_data);
    write(message, 0x10, saved_count);
}
} //namespace skeet::network
