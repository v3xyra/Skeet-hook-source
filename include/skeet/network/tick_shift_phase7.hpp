#pragma once
#include "tick_shift_runtime.hpp"
#include "tick_shift_observers.hpp"
#include "skeet/prediction/command.hpp"

namespace skeet {
class MovementCollisionEngine;
struct MovementEnvironmentBindings;
struct MovementCaptureInput;
struct RageStopBindings;
namespace anti_aim {
struct TargetState;
struct OtherGlobalSettings;
}
namespace script {
class SourceScriptRuntimeOwner;
}
namespace rage {
struct TargetBindings;
}
namespace prediction {
struct ObserverRegistry;
}
namespace client {
struct PredictionHistoryOwner;
struct CommandRecaptureContext;
class CommandCaptureRuntimeOwner;
struct CommandCaptureOffsets;
struct CommandTrigonometry;
struct GeneratedCommandOwnersBindings;
struct RestrictedPlayerCommandOwner;
struct RestrictedPlayerCommandBindings;
}
namespace network {
struct TickShiftPhaseState;

struct TickShiftPhase7Offsets {
    std::ptrdiff_t buttons_4746d093, previous_buttons_e765ed03;
    std::ptrdiff_t pressed_buttons_3db8eb39, released_buttons_356b64e8;
    std::ptrdiff_t command_copy_5df1a4bc, command_marker_a249061a;
};
struct TickShiftPhase7Bindings {
    const client::CommandCaptureRuntimeOwner& capture;
    const client::CommandCaptureOffsets& capture_offsets;
    const prediction::CommandBindings& prediction_bindings;
    const prediction::CommandPlayerOffsets& prediction_player_offsets;
    const prediction::ObserverRegistry& prediction_observers; //43475AB0/AB4
    const TickShiftRechargeObserverRegistry& generated_observers; //43475784/788
    prediction::SeedGenerator const& seed_generator; //actual434679AC
    std::int32_t* const& prediction_seed; //actual*43468328
    script::SourceScriptRuntimeOwner& scripts;
    const client::GeneratedCommandOwnersBindings& owners;
    std::span<const std::uint32_t, 150> inverted_commands;
    const anti_aim::OtherGlobalSettings& anti_aim_other; //canonical leg movement434685E8
    const client::CommandTrigonometry& trigonometry;
    const client::ScriptCommandEnvironment& angle_environment;
    const MovementCaptureInput& movement;
    const MovementEnvironmentBindings& movement_environment;
    const RageStopBindings& stopping;
    MovementCollisionEngine& collision;
    const rage::TargetBindings& targeting;
    const anti_aim::TargetState& target;
    const std::uint8_t& predictive_attack; //434671E0
    const HotkeyBinding& predictive_attack_key; //43467F34/43467F48
    const InputBitmap& input;
    const std::uint32_t& attack_policy; //4346A674
    std::uint32_t& attack_mask; //43468488
    std::int32_t& next_generated_command; //434759F4
    const std::int32_t& last_attack_command; //434759C4
    client::PredictionHistoryOwner* const& restricted_owner; //live434759B4
    const client::RestrictedPlayerCommandBindings& restricted;
    std::uint8_t* const& transform_override; //actual434683D0
    TickShiftPhase7Offsets offsets;
};

// Complete43359E37, with concrete source calls for generated movement/AA,
// firing, simulation, command capture and restricted animation. The caller
// supplies the original frame's separate send byte, retaining its x86 layout.
void process_tick_shift_phase7(TickShiftRuntimeState&, TickShiftPhaseState&, client::CommandFrame&,
                            void* player, client::Command&, std::uint8_t& send_packet,
                            const TickShiftRuntimeSettings&, TickShiftRuntimeBindings&,
                            TickShiftPhase7Bindings&, std::uint32_t& command_flags);
} //namespace network
} //namespace skeet
