#pragma once
#include "skeet/core.hpp"
#include "skeet/engine/interface_call.hpp"
#include "skeet/script/bridge.hpp"
#include <array>

namespace skeet::client {

// The complete 0x64-byte command copied by 433D333B. The leading word is an
// x86 vtable address, even when the recovered algorithms are built for x64.
struct Command {
    std::uint32_t vtable;
    std::int32_t number;
    std::int32_t tick;
    std::array<float, 3> angles;
    std::array<float, 3> aim_direction;
    UserCommand movement;
    std::uint8_t impulse;
    std::array<std::uint8_t, 3> impulse_padding;
    std::int32_t weapon_select;
    std::int32_t weapon_subtype;
    std::int32_t random_seed;
    std::int16_t mouse_x;
    std::int16_t mouse_y;
    std::uint8_t predicted;
    std::array<std::uint8_t, 3> predicted_padding;
    std::array<float, 3> saved_angles;
    std::uint32_t saved_buttons;
    std::array<std::uint32_t, 2> trailing;
};
static_assert(sizeof(Command) == 0x64);
static_assert(offsetof(Command, tick) == 8);
static_assert(offsetof(Command, angles) == 0xC);
static_assert(offsetof(Command, movement) == 0x24);
static_assert(offsetof(Command, saved_angles) == 0x4C);
static_assert(offsetof(Command, saved_buttons) == 0x58);

struct ChannelHookState {
private:
    void** standalone_expected_ = nullptr;
    void** standalone_replacement_ = nullptr;
    void* standalone_channel_ = nullptr;

public:
    ChannelHookState() noexcept
        : expected_table(standalone_expected_), replacement_table(standalone_replacement_),
          installed_channel(standalone_channel_) {}
    ChannelHookState(void**& expected, void**& replacement, void*& channel) noexcept
        : expected_table(expected), replacement_table(replacement), installed_channel(channel) {}
    ChannelHookState(const ChannelHookState&) = delete;
    ChannelHookState& operator=(const ChannelHookState&) = delete;
    ChannelHookState(ChannelHookState&&) = delete;
    ChannelHookState& operator=(ChannelHookState&&) = delete;
    void**& expected_table; // live43475B0C
    void**& replacement_table; // live43475B14
    void*& installed_channel; // live43475B08
};
// Complete 433CBDFD. Engine-client slot 78 obtains the channel. A replacement
// is installed only while its table still equals the recorded expected table.
void restore_channel_hook(void* engine_client, ChannelHookState&);

struct CommandEntryBindings {
    void* const& engine_client; // live4346A514
    void** original_player_table; // hook metadata's original vtable
    ChannelHookState* channel_hook;
    std::array<std::ptrdiff_t, 2> rejection_bytes; // 43468E78, 4346A5DC
};
enum class CommandPath : std::uint32_t {
    engine_return,
    empty_tick,
    active_command,
};
struct CommandEntry {
    std::uint8_t original_result;
    CommandPath path;
};
// Exact framing/gates 433D2E40..433D2F05, with the active path continuing at
// 433D2F95. Calls the original player CreateMove slot 289 before testing
// engine slots 82/93. The empty-tick path alone tests both rejection bytes.
// The later internal feature phases are separate recovered runtime units.
CommandEntry enter_create_move(const CommandEntryBindings&, void* player, float sample_time,
                               Command&);
//Production433D2E40 framing reads the current receiver's hook metadata AFTER
//channel restoration, then calls its genuine original player virtual289.
CommandEntry enter_hooked_create_move(const CommandEntryBindings&, void* player, float sample_time,
                                      Command&);

struct CommandEntityBindings {
    void* const& entity_list; // live4346A890
    std::ptrdiff_t overridden_property; // decoded 4346A808
    std::uint32_t* const& replacement_value; // live43469060
};
using CommandEntitySnapshot = std::array<std::uint32_t, 64>;
// Complete temporary entity override at 433D30CC..433D3174 and matching
// restoration 433D5953..433D59DB. Indices are 1..64, indexed by (index & 63).
// Native restoration intentionally skips saved zero values and rechecks both
// lookup and player virtual slot 158, retaining those observable side effects.
void save_command_entity_properties(const CommandEntityBindings&, CommandEntitySnapshot&);
void restore_command_entity_properties(const CommandEntityBindings&, const CommandEntitySnapshot&);

// Exact final command snapshot 433D5868..433D587A; copies the angle words
// sequentially, preserving all bit patterns, including NaNs.
void save_final_command(Command&) noexcept;

struct ScriptCommand {
    std::int32_t choked_commands;
    std::int32_t command_number;
    std::array<float, 3> angles;
    float forward_move, side_move;
    std::uint8_t send_packet, force_defensive;
    std::array<std::uint8_t, 2> padding;
    std::uint32_t buttons;
    std::int32_t weapon_select, weapon_subtype;
    float move_yaw;
    std::uint8_t tick_shift_state, state_bit_3, state_bit_18, state_bit_20;
};
static_assert(sizeof(ScriptCommand) == 0x34);
static_assert(offsetof(ScriptCommand, buttons) == 0x20);

// Exact snapshot and apply blocks of 43347D5C. The callback sees the same
// ScriptCommand object, which the native runtime wraps using registry slot 1.
struct ScriptCommandBaseline {
    std::uint8_t send_packet, tick_shift_state, state_bit_18;
};
ScriptCommandBaseline capture_script_command(ScriptCommand&, const Command&, float move_yaw,
                                             std::uint8_t send_packet, std::int32_t choked_commands,
                                             std::uint32_t scheduler_flags) noexcept;
void apply_script_command(ScriptCommand&, Command&, float& move_yaw, std::uint8_t& send_packet,
                          std::uint32_t& scheduler_flags, ScriptCommandBaseline,
                          bool unrestricted_angles, bool block_roll) noexcept;

struct ScriptCommandEnvironment {
    const std::uint8_t* angle_guard; // 434693EC
    void* const* shared_player; // 4346A3A4; may itself be null
    std::ptrdiff_t player_guard; // decoded 43468378
};
bool command_angles_unrestricted(const ScriptCommandEnvironment&) noexcept; // 433C0714
bool command_roll_blocked(const ScriptCommandEnvironment&) noexcept; // 433240A6
struct ScriptCommandBindings {
    script::Bridge* bridge;
    ScriptCommand* snapshot; // 43479300
    const char* event_name; // initialized 434795E0
    const std::int32_t* choked_commands; // *434757DC
    std::uint8_t* marshalling; // 43475A5C
    // Actual Lua 5.1 lua_pushlightuserdata, native entry 43429132.
    void (*push_light_userdata)(void*, void*);
    ScriptCommandEnvironment environment;
};
// Complete 43347D5C -> 433BA445 -> 4334AD41 dispatch/apply chain. Reuses the
// recovered callback-list dispatcher; the runtime Lua table's own constructor
// is called through its real registry reference, with no feature callbacks.
int dispatch_command_script(const ScriptCommandBindings&, Command&, float& move_yaw,
                            std::uint8_t& send_packet, std::uint32_t& scheduler_flags);

struct CommandAngleBindings {
    void* const& engine_client;
    void* const& prediction;
    ScriptCommandEnvironment environment;
};
// Complete 433B8974: sanitize NaNs, normalize under the recovered trust guard,
// clear roll, and call actual engine-client SetViewAngles (slot 19).
void set_command_view_angles(const CommandAngleBindings&, std::array<float, 3>&);
struct FreeLookState {
    std::array<std::uint32_t, 3> prefix;
    std::array<float, 3> delta;
    std::uint8_t active;
    std::array<std::uint8_t, 3> padding;
    std::array<float, 3> base_angles;
};
static_assert(sizeof(FreeLookState) == 0x28);
// Complete 4341B096; the unused first native stack argument is omitted.
// Uses the real hotkey evaluator, then engine slot 19 and, for tick zero,
// prediction SetLocalViewAngles (slot 13), in native order.
void process_command_free_look(FreeLookState&, Command&, const HotkeyBinding&, InputBitmap,
                               const CommandAngleBindings&);

} // namespace skeet::client
