#pragma once
#include <cstddef>
#include <cstdint>

namespace skeet::client {
struct Command;
}
namespace skeet::network {
struct NativeTickShiftRechargeReceiver;
#if defined(_M_IX86) || defined(__i386__)
using NativeTickShiftGeneratedEntry = void(__fastcall*)(NativeTickShiftRechargeReceiver*, void*,
                                                      std::int32_t, std::int32_t);
using NativeTickShiftRechargeEntry = void(__fastcall*)(NativeTickShiftRechargeReceiver*, void*,
                                                     std::int32_t);
using NativeTickShiftClonedEntry = void(__thiscall*)(NativeTickShiftRechargeReceiver*, void*,
                                                   client::Command*);
#else
using NativeTickShiftGeneratedEntry = void (*)(NativeTickShiftRechargeReceiver*, void*, std::int32_t,
                                             std::int32_t);
using NativeTickShiftRechargeEntry = void (*)(NativeTickShiftRechargeReceiver*, void*, std::int32_t);
using NativeTickShiftClonedEntry = void (*)(NativeTickShiftRechargeReceiver*, void*, client::Command*);
#endif
// Slot0 receives player in EDX and two stack words; slot1 receives player in
// EDX and one stack word. Slot2 consumes ECX and two stack words; its captured
// EDX is not an input to any of the five actual entries.
struct NativeTickShiftRechargeVtable {
    NativeTickShiftGeneratedEntry generated;
    NativeTickShiftRechargeEntry recharge;
    NativeTickShiftClonedEntry cloned;
};
struct NativeTickShiftRechargeReceiver {
    const NativeTickShiftRechargeVtable* vtable = nullptr;
    void* source = nullptr; //source bridge; the native pointer vector stores this endpoint
};
static_assert(offsetof(NativeTickShiftRechargeReceiver, vtable) == 0);
#if UINTPTR_MAX == UINT32_MAX
static_assert(sizeof(NativeTickShiftRechargeVtable) == 0xC);
static_assert(offsetof(NativeTickShiftRechargeVtable, recharge) == 4);
static_assert(offsetof(NativeTickShiftRechargeVtable, cloned) == 8);
static_assert(sizeof(NativeTickShiftRechargeReceiver) == 8);
#endif
} //namespace skeet::network
