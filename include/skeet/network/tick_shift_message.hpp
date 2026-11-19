#pragma once
#include "packet_control.hpp"
#include "skeet/misc/network_console_handlers.hpp"

namespace skeet::network {
// Genuine IBaseClientDLL virtual24 receives this native bf_write layout.
struct TickShiftCommandWriter {
    std::byte* data;
    std::int32_t data_bytes, data_bits, current_bit;
    std::uint8_t overflow, assert_on_overflow;
    std::uint16_t padding;
    const char* debug_name;
};
#if UINTPTR_MAX == UINT32_MAX
static_assert(sizeof(TickShiftCommandWriter) == 0x18);
static_assert(offsetof(TickShiftCommandWriter, current_bit) == 0xC);
static_assert(offsetof(TickShiftCommandWriter, debug_name) == 0x14);
#endif
struct TickShiftMessageBindings {
    void* const& client; //434683E0, virtual24 WriteUsercmdDeltaToBuffer
    void* const& input; //4346A894, actual command ring at+F0
    std::int32_t* const& choked; //434757DC
    std::int32_t* const& last_outgoing; //43475B2C
    void** const& original_channel_vtable; //43475B0C, original virtual40
};
// Complete4336F2EC, registered fullowner+10 in SendNetMsg registry43475B94.
// Internal command lookup stays concrete; only real engine virtuals7/24/40
// are boundaries. The original message count/data are restored before return.
void rewrite_tick_shift_command_message(const TickShiftState&, const TickShiftMessageBindings&,
                                     misc::NetworkMessageCall&);
struct TickShiftMessageHandler : misc::NetworkMessageCallback {
    TickShiftState& state;
    const TickShiftMessageBindings& bindings;
    TickShiftMessageHandler(TickShiftState&, const TickShiftMessageBindings&) noexcept;
    void publish_table() noexcept;
};
} //namespace skeet::network
