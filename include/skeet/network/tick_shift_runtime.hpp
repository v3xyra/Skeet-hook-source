#pragma once
#include "packet_control.hpp"
#include "skeet/client/command_frame.hpp"
#include "skeet/client/verified_command.hpp"
#include "skeet/prediction/history.hpp"
#include "skeet/prediction/observer_receiver.hpp"
#include "skeet/weapon/fire_state.hpp"
#include <cstring>

namespace skeet::rage {
struct TickShiftSettings;
}
namespace skeet::client {
struct PredictionHistoryOwner;
struct EntityCommandOwner;
}
namespace skeet::network {
class TickShiftRechargeObserverRegistry;
template <class Command> class TickShiftCommandRing {
public:
    TickShiftCommandRing(std::span<Command, 150> decoded) : decoded_(decoded.data()) {}
    TickShiftCommandRing(void* const& input, std::uint32_t displacement)
        : input_(&input), displacement_(displacement) {}
    Command& operator[](std::size_t index) const noexcept {
        if (!input_)
            return decoded_[index];
        Command* ring;
        std::memcpy(&ring, static_cast<const std::byte*>(*input_) + displacement_, sizeof ring);
        return ring[index];
    }

private:
    Command* decoded_ = nullptr;
    void* const* input_ = nullptr;
    std::uint32_t displacement_ = 0;
};

struct TickShiftCommandTick {
    std::int32_t command, tick;
};
struct TickShiftRuntimeState {
    TickShiftState& network; // same physical owner used by update_tick_shift_command
    std::int32_t field_f0 = 0;
    std::int32_t transition_command_110 = 0x7FFFFFFF; //constructor4347DEEB
    std::uint8_t low_health_120 = 0;
    std::int32_t field_130; // complete owner+148, reset4337D660
    std::int32_t last_flush_command_134 = 0;
    std::array<TickShiftCommandTick, 150> command_ticks; // native +1E8, stride8
    std::array<std::int32_t, 150> generated_commands; // native +698, stride4
    prediction::NativePredictionObserverReceiver prediction_receiver{};
};

using VerifiedCommand = client::VerifiedCommand;
struct TickShiftCommandSnapshot {
    std::int32_t command_number;
    std::array<std::byte, 0x538> fields_004;
};
static_assert(sizeof(TickShiftCommandSnapshot) == 0x53C);

using CommandChecksum = client::CommandChecksum;
struct TickShiftControlView {
    rage::TickShiftSettings& settings;
    const InputBitmap& keys;
};
struct TickShiftRuntimeSettings {
    const TickShiftControlView& controls;
    const HotkeyBinding& command_block;
    const std::uint8_t& special_weapon_43467560;
    const std::int32_t& configured_budget_43469274;
    const std::int32_t& process_milliseconds_43468290;
    const std::int32_t& health_divisor_4346db30;
};
struct TickShiftRuntimeOffsets {
    std::ptrdiff_t rule_bf567619, rule_dda99ab5, rule_b7ea8fda;
    std::ptrdiff_t player_block_9d920e35, player_health_9bb80163, tick_base_3d10b94c;
    std::ptrdiff_t weapon_byte_e1df7e9b, weapon_time_3b93667b, weapon_byte_e865a877;
};
struct TickShiftRuntimeBindings {
    void* const& engine_client; // 4346A514; actual IsInGame slot26
    void** const& game_rules; // 4346A3A4, nullable at either level
    const void* const& freeze_entity_43475b74;
    const void* const& shared_player_43475808;
    std::int32_t* const& choked_commands; // *434757DC, updated while cloning commands
    const std::int32_t* const& last_outgoing; // *43475B2C
    const std::int32_t* const& command_ack; // *43475BA4
    const float* const& tick_interval; // *4346A338
    float* const& current_time; // *43467EB8
    TickShiftTiming timing; // concrete43385D57/4340C3FA data, not callback replacements
    weapon::FireStateBindings firing;
    TickShiftRuntimeOffsets offsets;
    std::span<const std::uint32_t, 3> weapon_classification; // native43474580
    TickShiftCommandRing<client::Command> commands; // CInput +F0
    TickShiftCommandRing<VerifiedCommand> verified_commands; // CInput +F4
    CommandChecksum const& checksum; // source function43467C48, nullable natively
    client::PredictionHistoryOwner* const& prediction_snapshots_owner; // live nullable434759B4
    client::EntityCommandOwner* const& prediction_history_owner; // live nullable43475898
    const TickShiftRechargeObserverRegistry& recharge_observers; //actual43475784/88/8C
    const std::uint32_t* const& history_stamp_4346a334;
    void* const& prediction; // actual engine prediction43469264; reset+C/+1C/+24
};

// Complete4336663F on its native valid-local-player/valid-command-buffer
// contract. The separate send_packet reference is the actual byte addressed
// by frame.send_packet_address, preserving CommandFrame's x86 layout.
void process_tick_shift_runtime(TickShiftRuntimeState&, client::CommandFrame&, void* local_player,
                             void* weapon, client::Command&, std::uint8_t& send_packet,
                             const TickShiftRuntimeSettings&, TickShiftRuntimeBindings&,
                             std::uint32_t& command_flags);

} // namespace skeet::network
