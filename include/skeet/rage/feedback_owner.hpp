#pragma once
#include "feedback_command_owner.hpp"
#include "feedback_entity_owner.hpp"
#include "settings.hpp"
#include "skeet/client/command_phase_registry.hpp"
#include "skeet/client/entity_observer_registry.hpp"
#include "skeet/engine/decoded_offset.hpp"
#include "skeet/engine/startup_convars.hpp"
#include "skeet/engine/frame_update.hpp"
#include "skeet/events/registry.hpp"
#include "skeet/network/tick_shift_observers.hpp"
#include "skeet/rage/spread.hpp"

namespace skeet::client {
class NativeStartupSources;
}
namespace skeet::rage {
struct FeedbackPacketOffsets {
    engine::DecodedOffset player_index, origin, spread, inaccuracy, recoil_index;
    engine::DecodedOffset angles, seed, fire_mode, weapon_id;
};
FeedbackPacketOffsets feedback_packet_offsets(const engine::NativeOffsetStorage&);
struct FeedbackPacketBindings {
    const std::uint8_t& capture_enabled; //43467224
    const std::uint8_t& recording_enabled; //4346754C
    void* const& input; //4346A894, actual150-command ring
    engine::ConVarBinding shotgun_patterns; //live decoded43468520 ConVar
    FeedbackPacketOffsets offsets;
    const SpreadTables& spread_tables;
    const SpreadRandom& random;
    const ShotgunPattern& shotgun_pattern;
    const RageTrigonometry& trigonometry;
};

class FeedbackOwner;
struct FeedbackOwnerBindings {
    engine::OwnerRegistryBinding lifecycle; //43475B78
    client::CommandPhaseRegistry& commands; //434757A0
    client::EntityObserverRegistry& entities; //43475BA8
    engine::OwnerRegistryBinding packets; //4347584C
    engine::OwnerRegistryBinding frames; //43475AD8
    engine::OwnerRegistryBinding positions; //43475930/34/38
    network::TickShiftRechargeObserverRegistry& recharge; //43475784
    ConfigRegistries& configuration;
    events::EventRegistry& events;
    ShotLogSettings& settings;
    ShotFeedbackState& state;
    ShotFeedbackRuntime& runtime;
    const FeedbackEntityBindings& live;
    const FeedbackPacketBindings& packet;
    const std::int32_t& last_outgoing_command; //*43475B2C
    void* const& engine_client; //4346A514, captured once at4347F79C
    FeedbackOwner*& active; //canonical43475A7C, full primary object
};

std::size_t feedback_owner_size() noexcept;
// Complete ordinary4347F708 constructor. Bindings and all shared storage
// outlive the owner. State queues/descriptors are fresh and unregistered;
// no separate command/entity/recharge owner may be preregistered for them.
FeedbackOwner& construct_feedback_owner(void* native_aligned_storage, const FeedbackOwnerBindings&);
FeedbackOwner& construct_feedback_owner(void*, FeedbackOwnerBindings&&) = delete;
FeedbackOwner& construct_feedback_owner(void*, const FeedbackOwnerBindings&&) = delete;
// Complete43485C3A. Native allocation is110 bytes; mem_int3.h records the
//43485C53 MOV[43475A7C],EAX on both success and allocation failure.
FeedbackOwner* create_feedback_owner(const FeedbackOwnerBindings&);
FeedbackOwner* create_feedback_owner(FeedbackOwnerBindings&&) = delete;
FeedbackOwner* create_feedback_owner(const FeedbackOwnerBindings&&) = delete;
// Binds only the ordinary factory identity. Preserved startup metadata keeps
// its actual operand/order; this function does not execute or rewrite the VM.
void bind_feedback_owner_factory(client::NativeStartupSources&, FeedbackOwnerBindings&);
// Complete433A0098/433CDB60: clear active first, unsubscribe only the dynamic
// impact listener, release snapshot/event/text/pending storage, header-free.
void destroy_feedback_owner(FeedbackOwner*, FeedbackOwner*& active);
void* feedback_lifecycle_object(FeedbackOwner&) noexcept;
ShotFeedbackState* feedback_owner_state(FeedbackOwner*) noexcept;
const ShotFeedbackState* feedback_owner_state(const FeedbackOwner*) noexcept;
const ShotLogSettings* feedback_owner_settings(const FeedbackOwner*) noexcept;
const ShotFeedbackRuntime* feedback_owner_runtime(const FeedbackOwner*) noexcept;

void reset_feedback_owner(FeedbackOwner&) noexcept; //433C8E1D, primary7
void capture_feedback_packet(FeedbackOwner&, const void* packet); //complete433C7A41
void capture_feedback_position(FeedbackOwner&, const RageVector&) noexcept; //complete433ED9DA
void update_feedback_subscription(FeedbackOwner&, std::uint8_t update); //43331AE5
// Complete ordinary433A00C4..433A0124 caller: flags+1C require0B, then the
// three packet coordinates+8/+C/+14 are sent through the native XMM ABI.
void dispatch_feedback_position_packet(const engine::OwnerRegistry&, const void* packet);
struct FeedbackPositionRuntime {
    const engine::OwnerRegistry& positions; //actual43475930/34/38
};
void bind_feedback_position_runtime(FeedbackPositionRuntime&) noexcept;
void unbind_feedback_position_runtime(FeedbackPositionRuntime&) noexcept;
//433A00C4 reads its only argument from[EBP+8] and returns with plainRET.
// Its proven body does not invoke any original callback/trampoline.
#if defined(_M_IX86) || defined(__i386__)
void __cdecl feedback_position_hook(const void* packet);
#else
void feedback_position_hook(const void* packet);
#endif
} //namespace skeet::rage
