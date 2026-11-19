#pragma once
#include "commands.hpp"
#include "skeet/network/tick_shift_runtime.hpp"
#include "skeet/animation/entity_update.hpp"
#include "skeet/animation/local_helpers.hpp"
#include "skeet/client/player_state.hpp"
#include "skeet/engine/startup_convars.hpp"
#include "input_commands.hpp"

namespace skeet::prediction {
struct ObserverRegistry;
}

namespace skeet::client {

struct RestrictedCommandPrediction0x4C {
    std::int32_t command_number;
    std::uint32_t field_04;
    std::int32_t tick_base;
    float recoil_pitch;
    std::uint32_t field_10;
    std::array<float, 3> origin;
    std::uint32_t flags;
    std::uint8_t field_24;
    std::array<std::byte, 3> padding_25;
    float field_fb7e3404, field_3309c0cf;
    std::array<float, 3> velocity;
    float field_ae838771;
    std::uint32_t field_d4248210, field_a3abbb09;
    std::uint8_t field_7501cb91, field_7090d8dd, field_cacaeecc, field_a15c3105;
};
static_assert(sizeof(RestrictedCommandPrediction0x4C) == 0x4C);
static_assert(offsetof(RestrictedCommandPrediction0x4C, recoil_pitch) == 0xC);
static_assert(offsetof(RestrictedCommandPrediction0x4C, origin) == 0x14);
static_assert(offsetof(RestrictedCommandPrediction0x4C, velocity) == 0x30);

// Borrowed views of the single owner behind434759B4. The producer and its
// native storage remain shared with tick-shift cloning and RAGE history reads.
struct RestrictedPlayerCommandOwner {
    const std::int32_t* last_processed_command; // owner+38
    network::TickShiftCommandSnapshot* snapshots; // owner+3C,150 entries
    RestrictedCommandPrediction0x4C* prediction_records; // owner+40,150 entries
    const std::uint32_t* command_tags; // owner+1AC,150 entries
    const std::uint32_t* landing_tags; // owner+404,150 entries
    const char* moving_modifier; // owner+4C
    const char* crouching_modifier; // owner+5C
    const char* layer_modifier; // owner+7C
    const char* idle_activity; // owner+CC
    const char* idle_modifier; // owner+DC
    const char* jump_activity; // owner+EC
};

struct RestrictedPlayerCommandOffsets {
    lagcomp::EntityNetvars animation;
    std::ptrdiff_t field_66146802, field_00a5bd68, field_52899df3;
    std::ptrdiff_t field_1bae1168, field_f58e6118, weapon_handle, tick_base;
    std::ptrdiff_t field_36375c23, field_2a671b42, field_3309c0cf;
    std::ptrdiff_t field_e2fbe695, field_ac7f2913, field_7090d8dd;
    std::ptrdiff_t field_cacaeecc, field_a15c3105, field_ae838771;
    std::ptrdiff_t field_d4248210, field_a3abbb09, field_7501cb91;
    std::ptrdiff_t field_7a2d60c1, field_4e66bb34, field_a9ee6062;
    std::ptrdiff_t field_f8205eba;
};
class RestrictedCommandRing {
public:
    RestrictedCommandRing(std::span<Command, 150> commands) noexcept
        : commands_(commands.data()), input_(nullptr) {}
    explicit RestrictedCommandRing(void* const& input) noexcept
        : commands_(nullptr), input_(&input) {}
    Command& operator[](std::size_t slot) const noexcept {
        return input_ ? input_command_slot(*input_, std::uint32_t(slot)) : commands_[slot];
    }

private:
    Command* commands_;
    void* const* input_;
};
struct RestrictedPlayerCommandBindings {
    const std::uint8_t* enabled_43467224;
    const std::uint8_t* enabled_4346a740;
    animation::AnimationAngleHistoryView angle_history;
    engine::NativeConVarReference recoil_scale_convar;
    void* const& entity_list;
    PlayerStateCache& players;
    RestrictedCommandRing commands;
    EntityCommandOwner* const& history_owner; //live43475898, getter433952F0
    const float* interval;
    const std::int32_t* frame_number;
    float* current_time;
    float* globals_current_time; //43467B04+10; retains its real aliasing
    float* body_yaw_time; //43476B3C
    float* body_yaw; //43476B40
    std::array<float, 3>* sent_angles; //4347BD30
    prediction::ObserverRegistry* tickbase_observers; //43475AB0..43475AB4
    void** original_player_table; //original virtual224, recovered hook metadata
    std::uint8_t* animation_update_flag; //43475BD3
    animation::AnimationUpdateTiming animation_timing;
    const animation::AnimationUpdateEngine& animation_engine;
    const animation::LocalAnimationBindings& helpers;
    RestrictedPlayerCommandOffsets offsets;
    std::int32_t replay_restore_tick_scratch; //original incoming stack+188
    bool normalize_uninitialized_restore_time = false;
    bool resolve_original_player_table = false;
};
//43326621 ->433F54D5. Null inputs retain the wrapper's four early exits.
// caller_flag is the final native argument; the tag match is computed here
// from the same owner and command number immediately before the internal call.
void process_restricted_player_command(RestrictedPlayerCommandOwner*, void* player, Command*,
                                       std::uint8_t* send_packet, bool caller_flag,
                                       const RestrictedPlayerCommandBindings&);

} // namespace skeet::client
