#pragma once
#include "skeet/script/bridge.hpp"
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <excpt.h>
#include <csetjmp>
#include <cstdio>

namespace skeet::script::detail {
// Native frames mark Flags, retain Previous and place the saved Lua top after
// the TEB_ACTIVE_FRAME header. x86 checkpoints also save nonvolatile registers,
// ESP, EIP and FS:[0] immediately before that header.
struct ActiveFrame {
    std::uint32_t flags;
    ActiveFrame* previous;
    void* context;
};
extern "C" __declspec(dllimport) void __stdcall RtlPushFrame(ActiveFrame*);
extern "C" __declspec(dllimport) void __stdcall RtlPopFrame(ActiveFrame*);
extern "C" __declspec(dllimport) ActiveFrame* __stdcall RtlGetFrame();
struct X86Checkpoint {
    std::uint32_t edi, esi, ebp, esp, ebx, eip, seh;
};
static_assert(sizeof(X86Checkpoint) == 28);
struct CallbackFrame {
#if defined(__i386__) && defined(__clang__)
    X86Checkpoint checkpoint;
#else
    std::jmp_buf checkpoint;
#endif
    ActiveFrame active;
    int saved_top;
    std::uint32_t exception_code = 0;
    std::uintptr_t exception_address = 0;
};
#if defined(__i386__) && defined(__clang__)
static_assert(offsetof(CallbackFrame, active) == 28);
#endif

// Inner Lua protected calls handle their own errors. An unprotected library
// error reaches this outer frame and follows the native checkpoint cleanup.
#if defined(__i386__) && defined(__clang__)
// Keep the original register order and post-return ESP from 0x433F45FA.
// Clang's i386 MinGW __try lowering emits undefined EH-table labels; this
// explicit x86 registration uses the Windows dispatcher directly instead.
__attribute__((naked, noinline, returns_twice, fastcall)) inline int
save_checkpoint(X86Checkpoint*) {
    __asm__ __volatile__("movl %edi, 0(%ecx)\n\t"
                         "movl %esi, 4(%ecx)\n\t"
                         "movl %ebp, 8(%ecx)\n\t"
                         "leal 4(%esp), %edx\n\t"
                         "movl %edx, 12(%ecx)\n\t"
                         "movl %ebx, 16(%ecx)\n\t"
                         "movl (%esp), %edx\n\t"
                         "movl %edx, 20(%ecx)\n\t"
                         "movl %fs:0, %eax\n\t"
                         "movl %eax, 24(%ecx)\n\t"
                         "xorl %eax, %eax\n\t"
                         "retl");
}

// Native 0x4337EB9B. The continuation reports a nonzero checkpoint result.
[[noreturn]] __attribute__((naked, noinline, fastcall)) inline void
resume_checkpoint(const X86Checkpoint*) {
    __asm__ __volatile__("movl 0(%ecx), %edi\n\t"
                         "movl 4(%ecx), %esi\n\t"
                         "movl 8(%ecx), %ebp\n\t"
                         "movl 16(%ecx), %ebx\n\t"
                         "movl 24(%ecx), %eax\n\t"
                         "movl %eax, %fs:0\n\t"
                         "movl 12(%ecx), %esp\n\t"
                         "movl 20(%ecx), %edx\n\t"
                         "movl $1, %eax\n\t"
                         "jmpl *%edx");
}

inline void set_exception_chain(std::uint32_t chain) {
    __asm__ __volatile__("movl %0, %%fs:0" : : "r"(chain) : "memory");
}

struct X86Registration {
    X86Registration* previous;
    EXCEPTION_DISPOSITION(__cdecl* handler)(EXCEPTION_RECORD*, void*, CONTEXT*, void*);
    X86Checkpoint* checkpoint;
};

inline EXCEPTION_DISPOSITION __cdecl lua_exception_handler(EXCEPTION_RECORD* exception, void* frame,
                                                           CONTEXT* context, void*) {
    if ((exception->ExceptionCode & 0xffffff00u) != 0xe24c4a00u ||
        (exception->ExceptionFlags & 6u) != 0)
        return ExceptionContinueSearch;
    const auto& checkpoint = *static_cast<X86Registration*>(frame)->checkpoint;
    context->Edi = checkpoint.edi;
    context->Esi = checkpoint.esi;
    context->Ebp = checkpoint.ebp;
    context->Esp = checkpoint.esp;
    context->Ebx = checkpoint.ebx;
    context->Eip = checkpoint.eip;
    context->Eax = 1;
    set_exception_chain(checkpoint.seh);
    return ExceptionContinueExecution;
}

template <class Function> bool protected_region(Function& function) {
    X86Checkpoint checkpoint{};
    if (save_checkpoint(&checkpoint) != 0) {
        // The OS dispatcher temporarily links its own handler registration;
        // its return path can restore our registration after the handler has
        // resumed this checkpoint. Unlink once control is back in this frame.
        set_exception_chain(checkpoint.seh);
        return false;
    }
    X86Registration registration{reinterpret_cast<X86Registration*>(checkpoint.seh),
                                 lua_exception_handler, &checkpoint};
    set_exception_chain(reinterpret_cast<std::uint32_t>(&registration));
    try {
        function();
    } catch (...) {
        set_exception_chain(checkpoint.seh);
        throw;
    }
    set_exception_chain(checkpoint.seh);
    return true;
}
#else
template <class Function> bool protected_region(Function& function) {
    __try {
        function();
        return true;
    } __except ((GetExceptionCode() & 0xffffff00u) == 0xe24c4a00u ? EXCEPTION_EXECUTE_HANDLER
                                                                  : EXCEPTION_CONTINUE_SEARCH) {
        return false;
    }
}
#endif

// Each native callback frame keeps its panic continuation immediately before
// the magic TEB_ACTIVE_FRAME header. Normal Lua SEH and lua_atpanic therefore
// both leave the body through the same stack/context restoration by its caller.
template <class Function> bool protected_region(Function& function, CallbackFrame& frame) {
#if defined(__i386__) && defined(__clang__)
    if (save_checkpoint(&frame.checkpoint) != 0) {
        set_exception_chain(frame.checkpoint.seh);
        return false;
    }
#else
    if (setjmp(frame.checkpoint) != 0)
        return false;
#endif
    return protected_region(function);
}

[[noreturn]] inline void resume_panic_frame(ActiveFrame* active) {
    auto* frame = reinterpret_cast<CallbackFrame*>(reinterpret_cast<std::byte*>(active) -
                                                   offsetof(CallbackFrame, active));
#if defined(__i386__) && defined(__clang__)
    resume_checkpoint(&frame->checkpoint);
#else
    std::longjmp(frame->checkpoint, 1);
#endif
}
inline void report_lua_error(Bridge& bridge, void* state) {
    const auto& api = *bridge.api;
    if (const auto error = api.tolstring(state, -1, nullptr))
        if (bridge.report_error)
            bridge.report_error(bridge.error_context, error);
    api.settop(state, -2);
}
inline void report_lua_error(Bridge& bridge) {
    report_lua_error(bridge, bridge.state);
}
inline void report_native_exception(Bridge& bridge, const CallbackFrame& frame,
                                    const char* operation) {
    if (!frame.exception_code || !bridge.report_error)
        return;
    std::array<char, 256> message{};
    std::snprintf(message.data(), message.size(), "Native exception 0x%08X at 0x%08X while %s",
                  frame.exception_code, static_cast<unsigned>(frame.exception_address), operation);
    bridge.report_error(bridge.error_context, message.data());
}
} // namespace skeet::script::detail
