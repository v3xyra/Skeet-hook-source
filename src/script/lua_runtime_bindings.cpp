#include "skeet/script/lua_runtime_bindings.hpp"
#include "initialize_detail.hpp"
extern "C" {
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
#include "luajit.h"
void skeet_lua_runtime_layout(unsigned int output[3]);
}
#include <type_traits>
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <psapi.h>
#include <cstdio>
#include <cstring>
#include <limits>

namespace skeet::script {
namespace {
static_assert(sizeof(void*) == 4 && sizeof(lua_Integer) == sizeof(std::ptrdiff_t));
static_assert(std::is_same_v<lua_Number, double>);
static_assert(sizeof(LuaApi::CFunction) == sizeof(lua_CFunction));
static_assert(sizeof(CjsonRegistration) == sizeof(luaL_Reg));
static_assert(offsetof(CjsonRegistration, name) == offsetof(luaL_Reg, name));
static_assert(offsetof(CjsonRegistration, function) == offsetof(luaL_Reg, func));

// The bridge's opaque state has the same x86 cdecl ABI as lua_State*.
// Ordinary entries retain their exact public Lua parameter and result types.
template <auto Function> struct Entry;
template <class Result, class... Args, Result (*Function)(lua_State*, Args...)>
struct Entry<Function> {
    static Result invoke(void* state, Args... args) {
        return Function(static_cast<lua_State*>(state), args...);
    }
};
int cpcall(void* state, LuaApi::CFunction function, void* context) {
    return lua_cpcall(static_cast<lua_State*>(state), reinterpret_cast<lua_CFunction>(function),
                      context);
}
void pushcclosure(void* state, LuaApi::CFunction function, int upvalues) {
    lua_pushcclosure(static_cast<lua_State*>(state), reinterpret_cast<lua_CFunction>(function),
                     upvalues);
}
LuaApi::CFunction atpanic(void* state, LuaApi::CFunction function) {
    return reinterpret_cast<LuaApi::CFunction>(
        lua_atpanic(static_cast<lua_State*>(state), reinterpret_cast<lua_CFunction>(function)));
}
void* newstate() {
    return luaL_newstate();
}
void open_library(void* state, const char* name, const CjsonRegistration* entries, int upvalues) {
    // Both public registration records contain {name, x86 cdecl callback}.
    luaL_openlib(static_cast<lua_State*>(state), name, reinterpret_cast<const luaL_Reg*>(entries),
                 upvalues);
}

LuaRuntimeBindings make_bindings() noexcept {
    LuaRuntimeBindings result{};
    auto& lua = result.lua;
    lua.gettop = Entry<&lua_gettop>::invoke;
    lua.settop = Entry<&lua_settop>::invoke;
    lua.rawgeti = Entry<&lua_rawgeti>::invoke;
    lua.type = Entry<&lua_type>::invoke;
    lua.createtable = Entry<&lua_createtable>::invoke;
    lua.pushlstring = Entry<&lua_pushlstring>::invoke;
    lua.pushinteger = Entry<&lua_pushinteger>::invoke;
    lua.settable = Entry<&lua_settable>::invoke;
    lua.pcall = Entry<&lua_pcall>::invoke;
    lua.tolstring = Entry<&lua_tolstring>::invoke;
    lua.unref = Entry<&luaL_unref>::invoke;
    lua.pushvalue = Entry<&lua_pushvalue>::invoke;
    lua.ref = Entry<&luaL_ref>::invoke;
    lua.pushstring = Entry<&lua_pushstring>::invoke;
    lua.gettable = Entry<&lua_gettable>::invoke;
    lua.objlen = Entry<&lua_objlen>::invoke;
    lua.rawequal = Entry<&lua_rawequal>::invoke;
    lua.rawseti = Entry<&lua_rawseti>::invoke;
    lua.pushnil = Entry<&lua_pushnil>::invoke;
    lua.checktype = Entry<&luaL_checktype>::invoke;
    lua.pushnumber = Entry<&lua_pushnumber>::invoke;
    lua.call = Entry<&lua_call>::invoke;
    lua.remove = Entry<&lua_remove>::invoke;
    lua.gc = Entry<&lua_gc>::invoke;
    lua.close = Entry<&lua_close>::invoke;
    lua.getfield = Entry<&lua_getfield>::invoke;
    lua.next = Entry<&lua_next>::invoke;
    lua.rawset = Entry<&lua_rawset>::invoke;
    lua.toboolean = Entry<&lua_toboolean>::invoke;
    lua.isstring = Entry<&lua_isstring>::invoke;
    lua.checklstring = Entry<&luaL_checklstring>::invoke;
    lua.checkinteger = Entry<&luaL_checkinteger>::invoke;
    lua.checknumber = Entry<&luaL_checknumber>::invoke;
    lua.pushlightuserdata = Entry<&lua_pushlightuserdata>::invoke;
    lua.touserdata = Entry<&lua_touserdata>::invoke;
    lua.setfield = Entry<&lua_setfield>::invoke;
    lua.argerror = Entry<&luaL_argerror>::invoke;
    lua.tonumber = Entry<&lua_tonumber>::invoke;
    lua.tointeger = Entry<&lua_tointeger>::invoke;
    lua.pushboolean = Entry<&lua_pushboolean>::invoke;
    lua.newuserdata = Entry<&lua_newuserdata>::invoke;
    lua.getmetatable = Entry<&lua_getmetatable>::invoke;
    lua.setmetatable = Entry<&lua_setmetatable>::invoke;
    lua.checkudata = Entry<&luaL_checkudata>::invoke;
    lua.isnumber = Entry<&lua_isnumber>::invoke;
    lua.error = Entry<&lua_error>::invoke;
    lua.insert = Entry<&lua_insert>::invoke;
    lua.lcheckstack = Entry<&luaL_checkstack>::invoke;
    lua.where = Entry<&luaL_where>::invoke;
    lua.concat = Entry<&lua_concat>::invoke;
    lua.cpcall = cpcall;
    lua.pushcclosure = pushcclosure;
    // Varargs stay at the actual library entry; no formatting/argument replay.
    lua.lerror = reinterpret_cast<decltype(lua.lerror)>(&luaL_error);
    unsigned int layout[3]{};
    skeet_lua_runtime_layout(layout);
    result.initialization = {newstate,
                             Entry<&luaJIT_setmode>::invoke,
                             atpanic,
                             Entry<&luaL_openlibs>::invoke,
                             {layout[0], layout[1], layout[2]}};
    result.json = {lua,
                   Entry<&lua_typename>::invoke,
                   Entry<&lua_tonumber>::invoke,
                   Entry<&lua_newuserdata>::invoke,
                   Entry<&lua_setmetatable>::invoke,
                   Entry<&lua_checkstack>::invoke,
                   Entry<&lua_pushboolean>::invoke,
                   Entry<&luaL_checkoption>::invoke,
                   open_library};
    result.vector = {Entry<&luaL_loadbuffer>::invoke, Entry<&luaL_findtable>::invoke};
    return result;
}
} // namespace

const LuaRuntimeBindings& source_lua_runtime_bindings() noexcept {
    static const LuaRuntimeBindings bindings = make_bindings();
    return bindings;
}
} // namespace skeet::script

// Native43439986 checks the builtin-loading byte after detecting binary input.
// Resolve the canonical runtime by the VM's shared global reference, so a
// coroutine receives the same gate as its main state. Unowned states have no
// client builtin-loading permission.
extern "C" int skeet_lua_bytecode_allowed(lua_State* state) {
    auto* runtime = skeet::script::initialize_detail::owner(state);
    return runtime && runtime->loading_builtin;
}

namespace {
constexpr std::uintptr_t legacy_begin = 0x43310000u, legacy_end = 0x4360c000u;
constexpr DWORD readable = PAGE_READONLY | PAGE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_READ |
                           PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;
constexpr DWORD writable = PAGE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_READWRITE |
                           PAGE_EXECUTE_WRITECOPY;
constexpr DWORD executable = PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE |
                             PAGE_EXECUTE_WRITECOPY;

struct AddressRange {
    std::uintptr_t begin;
    std::size_t size;
};

// These ranges select the cheaper Windows query. They never cache access rights.
thread_local struct {
    AddressRange ranges[4]{};
    unsigned next = 0;
} large_allocations;

std::size_t accessible_span(std::uintptr_t address, DWORD access, std::size_t request) {
    // The client and LuaJIT are built exclusively for Windows x86 (4 KiB pages).
    constexpr std::size_t page_size = 4096;
    const auto page_left = page_size - (address & (page_size - 1));
    if (access == readable && request <= page_left) {
        for (const auto& range : large_allocations.ranges) {
            if (address - range.begin >= range.size)
                continue;
            PSAPI_WORKING_SET_EX_INFORMATION page{};
            page.VirtualAddress = reinterpret_cast<void*>(address);
            if (QueryWorkingSetEx(GetCurrentProcess(), &page, sizeof(page)) &&
                page.VirtualAttributes.Valid) {
                const auto protection = page.VirtualAttributes.Win32Protection;
                return !(protection & (PAGE_GUARD | PAGE_NOACCESS)) && (protection & readable)
                           ? page_left : 0;
            }
            // Nonresident pages have no working-set protection field.
            break;
        }
    }
    MEMORY_BASIC_INFORMATION region{};
    if (!VirtualQuery(reinterpret_cast<void*>(address), &region, sizeof(region)) ||
        region.State != MEM_COMMIT || (region.Protect & (PAGE_GUARD | PAGE_NOACCESS)) ||
        !(region.Protect & access))
        return 0;
    const auto base = reinterpret_cast<std::uintptr_t>(region.BaseAddress);
    if (region.RegionSize >= 4 * 1024 * 1024 && access == readable) {
        const auto allocation = reinterpret_cast<std::uintptr_t>(region.AllocationBase);
        // VirtualQuery scans from the queried page to the end of the region.
        // Earlier pages in that allocation may also benefit from a page query.
        const AddressRange range{allocation, (base - allocation) + region.RegionSize};
        auto& slot = large_allocations.ranges[large_allocations.next];
        slot = range;
        large_allocations.next = (large_allocations.next + 1) % 4;
    }
    return region.RegionSize - (address - base);
}
} // namespace

// Preserve the former image exclusion even when another allocation occupies it.
extern "C" int skeet_lua_ffi_access(const void* pointer, std::size_t length, int mode) {
    const auto first = reinterpret_cast<std::uintptr_t>(pointer);
    if (mode == 3) // Pointer argument: null and OS pseudo-handles remain legal.
        return first < legacy_begin || first >= legacy_end;
    if (!length)
        return 1;
    if (length - 1 > std::numeric_limits<std::uintptr_t>::max() - first)
        return 0;
    const auto last = first + length - 1;
    if (first < legacy_end && last >= legacy_begin)
        return 0;
    const auto access = mode == 2 ? executable : mode == 1 ? writable : readable;
    for (auto address = first;;) {
        const auto available = accessible_span(address, access, length);
        if (!available)
            return 0;
        if (available >= length)
            return 1;
        length -= available;
        address += available;
    }
}

// The trace recorder also uses this bounded scan; strlen must not cross into
// an inaccessible page before an FFI guard has a chance to run.
extern "C" int skeet_lua_ffi_strlen(const char* text) {
    std::size_t length = 0;
    for (;;) {
        const auto address = reinterpret_cast<std::uintptr_t>(text);
        if (address >= legacy_begin && address < legacy_end)
            return -1;
        // Short strings need only their first page. For longer strings query a
        // contiguous region once instead of querying every subsequent page.
        auto available = accessible_span(address, readable, length ? SIZE_MAX : 1);
        if (!available)
            return -1;
        if (address < legacy_begin && available > legacy_begin - address)
            available = legacy_begin - address;
        const auto* terminator = static_cast<const char*>(std::memchr(text, 0, available));
        const auto count = terminator ? static_cast<std::size_t>(terminator - text) : available;
        if (count > static_cast<std::size_t>(INT_MAX) - length)
            return -1;
        length += count;
        if (terminator)
            return static_cast<int>(length);
        if (available > UINTPTR_MAX - address)
            return -1;
        text = reinterpret_cast<const char*>(address + available);
    }
}

extern "C" void skeet_lua_ffi_unavailable(lua_State* state, const void* pointer, int mode) {
    auto* runtime = skeet::script::initialize_detail::owner(state);
    if (!runtime || !runtime->bridge.report_error)
        return;
    const auto address = reinterpret_cast<std::uintptr_t>(pointer);
    if (!runtime->unavailable_ffi_addresses.insert(address).second)
        return;
    const char* operation = mode == 2   ? "call"
                            : mode == 1 ? "write"
                            : mode == 3 ? "call argument"
                                        : "read";
    lua_Debug location{};
    const bool located = lua_getstack(state, 1, &location) && lua_getinfo(state, "Sl", &location);
    char message[512]{};
    std::snprintf(message, sizeof(message), "FFI: address 0x%08X unavailable; %s skipped%s%s:%d",
                  static_cast<unsigned>(address), operation, located ? " in " : "",
                  located ? location.short_src : "FFI", located ? location.currentline : 0);
    runtime->bridge.report_error(runtime->bridge.error_context, message);
}
