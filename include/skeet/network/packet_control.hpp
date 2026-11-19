#pragma once
#include "skeet/core.hpp"
#include <array>
#include <cstdint>

namespace skeet::network {
struct Vector {
    float x, y, z;
};

// Offsets refer to the secondary owner pointer passed to 0x433A069B.
// The first three fields alias the registered descriptor payloads in that
// owner. Bind them to FakeLagSettings.amount/variance/limit.value; a command
// phase must never copy those controls before an engine or random call.
struct FakeLagState {
    const std::int32_t& mode; // +014
    const std::int32_t& variance; // +028
    const std::int32_t& configured_cap; // +03C
    std::uint32_t flags; // +040
    float target_trigger_time; // +044
    std::int32_t cycle; // +048
    std::int32_t target; // +04C
    std::int32_t selected; // +050
    std::int32_t recorded_choked; // +054
    std::int32_t value_58; // +058
    float last_trigger_time; // +05C
    std::int32_t tracked_target; // +060
    std::int32_t previous_selected; // +064
    Vector last_origin; // +068
    std::uint32_t saved_duck_amount_bits; // +074; raw3309C0CF field
    float previous_speed_delta; // +078
    std::int32_t value_7c; // +07C
    Vector acceleration; // +080 (z is explicitly zeroed)
    std::int32_t value_8c; // +08C
};
struct PacketFrame {
    std::uint32_t flags;
    std::int32_t limit; // +0FC
    std::int32_t choked; // +15C
    Vector velocity; // +150
    Vector previous_velocity; // +180
    Vector origin; // +10C
    float previous_speed; // +190
};
struct RandomInterface {
    void* context;
    void (*seed)(void*, std::int32_t); // external vstdlib!RandomSeed
    std::int32_t (*integer)(void*, std::int32_t, std::int32_t); // RandomInt
};
struct PacketCommandContext {
    std::int32_t engine_choked;
    bool force_on_next_command; // global 43468D7C
    std::int32_t last_command; // global 434759C4
    std::int32_t command_number;
    Vector player_origin; // decoded actual local-player field
    std::uint32_t player_3309c0cf;
};
void reset_fake_lag(FakeLagState&) noexcept; // complete 43369ACB
void prepare_fake_lag_command(FakeLagState&, const PacketCommandContext&,
                              std::uint32_t& command_flags) noexcept; // 433EA911..433EA9DA
struct FakeLagActivation {
    const std::uint8_t& enabled; // 43468D64
    const HotkeyBinding& key; // 43467E20/24
    const InputBitmap& input;
};
bool fake_lag_active(const FakeLagActivation&); // complete 4333FD0C + native hotkey handlers
struct DuckPacketInput {
    std::uint8_t player_state_7501cb91;
    std::uint8_t player_state_ac7f2913;
    std::uint8_t player_state_f3c10e80;
    std::uint32_t player_flags;
    Vector velocity;
    Vector absolute_origin; // actual player virtual +028
    float interval;
    const std::int32_t& mode; // 4346A580
    std::int32_t engine_choked;
    FakeLagActivation activation;
};
void update_packet_duck(FakeLagState&, const DuckPacketInput&, std::uint32_t& buttons,
                        std::uint32_t& command_flags); // 433EA9DA..433EADAB
// Complete 433EA911 on its valid local-player contract.
void process_fake_lag_command(FakeLagState&, const PacketCommandContext&, const DuckPacketInput&,
                              std::uint32_t& buttons, std::uint32_t& command_flags);

// Contiguous native blocks. The target/weapon trace predicates between the
// selection and final send gate remain their callers' responsibility.
void begin_fake_lag_tick(FakeLagState&, const PacketFrame&, bool& trace_result) noexcept;
// Returns the branch chosen at433A0732: false means Maximum and skips the
// target/weapon middle. This branch cannot be reselected after a random call
// changes the live Amount control, although433A08E0 reads that control again.
bool select_fake_lag_limit(FakeLagState&, const PacketFrame&, float tick_interval,
                           const RandomInterface&); // 433A0732..433A0903
void apply_fake_lag_send_gate(const FakeLagState&, const PacketFrame&, std::uint32_t command_flags,
                              std::uint8_t& send_packet) noexcept;
void finish_fake_lag_tick(FakeLagState&, const PacketFrame&, std::int32_t target,
                          std::int32_t target_state) noexcept;
bool fake_lag_pending(std::int32_t configured_limit, const PacketFrame&,
                      std::uint32_t command_flags) noexcept; // complete 433FFD63
bool velocity_trigger(const FakeLagState&, const PacketFrame&) noexcept; // complete 433861F7
bool displacement_trigger(const FakeLagState&, const PacketFrame&, float speed,
                          std::uint32_t buttons, float tick_interval,
                          float gravity) noexcept; // complete 4340B019
struct PacketWeaponBindings {
    void* entity_list;
    std::ptrdiff_t reload_identifier; // CA5EC698
    std::ptrdiff_t reload_counter; // 49C770CA
    std::ptrdiff_t next_secondary; // C0D5F8A1
};
// These predicates retain their actual engine virtual queries. The native
// reload path requires a weapon whenever its 0x2000 button is set.
bool reload_trigger(const PacketWeaponBindings&, void* weapon, std::uint32_t buttons);
bool secondary_attack_trigger(const PacketWeaponBindings&, void* weapon, std::uint32_t buttons,
                              float current_time);
bool selected_weapon_trigger(
    const PacketWeaponBindings&, std::uint8_t current_weapon_type,
    std::uint32_t selected_weapon); // complete 433E3041 / 4333C7AC / 433CCBA8
bool classified_packet_weapon(std::uint16_t weapon_id,
                              std::span<const std::uint32_t, 3>) noexcept; // 433D24BB
void record_fake_lag_attack(FakeLagState&, const PacketFrame&, std::int32_t weapon_id,
                            std::uint8_t weapon_type, std::uint32_t buttons,
                            std::int32_t last_outgoing,
                            std::span<const std::uint32_t, 3> weapon_classification,
                            std::uint32_t& command_flags) noexcept; // complete433A0EFE

struct LagTriggerState {
    std::uint32_t flags; // primary owner +048
    float last_time; // +04C
    std::int32_t target; // +068
};
bool trigger_target_change(LagTriggerState&, float speed, std::int32_t target,
                           std::int32_t target_state, float current_time,
                           std::uint32_t& command_flags) noexcept; // 43394735

struct LatencySamples {
    std::uint32_t first;
    std::int32_t count;
    std::array<float, 16> values;
};
std::int32_t average_latency_ticks(const LatencySamples&) noexcept; // 43385D57
std::int32_t adjusted_command_tick(std::int32_t base, bool have_ack, std::int32_t ack,
                                   bool have_sent, std::int32_t sent) noexcept; // 4340C3FA
std::int32_t network_command_budget(bool have_rules, bool rule_0, bool rule_1,
                                    std::int32_t rule_mode,
                                    std::int32_t configured) noexcept; // 433A13ED

struct ShuffleRandom {
    std::int32_t seed;
    std::int32_t previous;
    std::array<std::int32_t, 32> table;
};
std::int32_t next_shuffle_random(ShuffleRandom&) noexcept; // complete 43387E35

// Offsets below refer to the tick-shift network interface (full owner +0x18).
struct TickShiftState {
    std::uint32_t flags; // +014
    std::int32_t fake_lag_limit; // +064
    std::int32_t mode; // +0F4: 1 double tap, 2 on shot, 3 fake peek
    std::int32_t minimum; // +0F8
    std::int32_t limit; // +0FC
    std::int32_t budget; // +100
    std::int32_t available; // +104
    std::int32_t last_command; // +108
    std::int32_t shot_tick; // +114
    float timer_124; // +124; HUD interface +134
    std::int32_t shot_command; // +128
    ShuffleRandom random; // +138
    std::uint32_t history_first; // +1C0
    std::int32_t history_count; // +1C4
    std::array<std::int32_t, 8> history; // +1C8
};
struct TickShiftGates {
    bool connected;
    bool checksum_available;
    bool freeze_period;
    bool freeze_entity_present;
    std::int32_t freeze_entity_state;
    bool player_blocked;
    bool command_blocked;
    bool fake_peek_active;
    bool double_tap_active;
    bool on_shot_active;
};
// Query phase9 forwards the signed running EDX limit through each owner in
// registration order. These functions return that owner's concrete result.
std::int32_t cap_fake_lag_commands(const FakeLagState&,
                                   std::int32_t running_limit) noexcept; // 4340354C
std::int32_t cap_tick_shift_commands(const TickShiftState&,
                                  std::int32_t running_limit) noexcept; // 4340165F
// Prefix 43366670..43366838. Activation inputs are the actual results of
// the three native hotkey descriptors, in native fake-peek/double-tap/on-shot order.
std::int32_t pending_tick_shift_commands(const TickShiftState&, std::int32_t choked) noexcept;
void select_tick_shift_mode(TickShiftState&, std::int32_t new_budget, const TickShiftGates&) noexcept;

struct TickShiftFrame {
    std::uint32_t flags; // +000
    std::uint32_t movement_flags; // +044
    bool has_weapon; // +00C
    std::int32_t tick; // +0AC
    std::int32_t tick_base; // +0C8
    std::int32_t saved_tick_base; // +0CC
    std::int32_t weapon_id; // +0D8
    std::uint8_t weapon_type; // +124
    std::int32_t tick_shift_mode; // +1D4
    std::int32_t next_tick; // +1D8
    std::int32_t choke_end; // +1E0
};
struct TickShiftCommand {
    std::int32_t number;
    float pitch, yaw;
    std::uint32_t buttons;
};
struct TickShiftTiming {
    std::int32_t choked;
    std::int32_t last_outgoing;
    std::int32_t process_milliseconds;
    float interval;
    float current_time;
    float player_next_attack;
    float weapon_next_attack;
    bool freeze_period;
    LatencySamples latency;
    std::int32_t command_tick;
    bool have_ack;
    std::int32_t acknowledged;
    bool have_sent;
    std::int32_t sent;
};
// Complete 433CE4BC, including its real latency helpers and shuffle PRNG.
// A valid local player is a native precondition; null-player obfuscation is unreachable.
void update_tick_shift_command(TickShiftState&, TickShiftFrame&, TickShiftCommand&, const TickShiftTiming&,
                            std::uint32_t& command_flags) noexcept;
void apply_tick_shift_weapon_gate(const TickShiftState&, std::int32_t last_outgoing, bool enabled,
                               std::uint8_t weapon_type, std::int32_t choked,
                               std::uint32_t& frame_flags, std::uint32_t& command_flags,
                               std::uint8_t& send_packet) noexcept; // complete 433B56ED
} // namespace skeet::network
