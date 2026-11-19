#include "skeet/client/command_owner_dispatch.hpp"
#include "skeet/anti_aim/command_owner.hpp"
#include "skeet/network/command_owner.hpp"
#include "skeet/network/tick_shift_command_owner.hpp"
#include "skeet/misc/command_owner.hpp"
#include "skeet/misc/grenade_command.hpp"
#include "skeet/misc/viewmodel_lag.hpp"
#include "skeet/misc/no_fall_damage.hpp"
#include "skeet/misc/low_fps_runtime.hpp"
#include "skeet/rage/command_source.hpp"
#include "skeet/rage/feedback_command_owner.hpp"
#include "skeet/effects/bullet_impact_command_owner.hpp"
#include "skeet/client/prediction_history_owner.hpp"
#include "skeet/client/entity_command_owner.hpp"
#include "skeet/client/buffered_shot_command.hpp"
#include "skeet/script/command_owner.hpp"
#include "skeet/skins/command.hpp"
#include "skeet/client/target_mask_owner.hpp"
#include "skeet/movement/command_wrapper.hpp"
#include <stdexcept>

namespace skeet::client {
namespace {
#if defined(_M_IX86) || defined(__i386__)
#define COMMAND_FAST __fastcall
#else
#define COMMAND_FAST
#endif
template <class T> T& source(NativeCommandPhaseReceiver* receiver) {
    return *static_cast<T*>(receiver->source);
}
std::uint8_t& send_packet(CommandFrame& frame) {
    return *reinterpret_cast<std::uint8_t*>(std::uintptr_t(frame.send_packet_address));
}
void* weapon_pointer(const CommandFrame& frame) {
    return reinterpret_cast<void*>(std::uintptr_t(frame.weapon_address));
}
void COMMAND_FAST noop_phase0(NativeCommandPhaseReceiver*, void*, Command*) {} //433C95B7 RET4
void COMMAND_FAST noop_phase1(NativeCommandPhaseReceiver*, void*, void*, Command*, std::uint8_t*) {
} //4337D653 RET0C
void COMMAND_FAST noop_phase2(NativeCommandPhaseReceiver*, void*, void*, Command*,
                              std::array<float, 3>*, std::uint8_t*) {} //43337B12 RET10
void COMMAND_FAST noop_frame_phase(NativeCommandPhaseReceiver*, CommandFrame*, void*, Command*) {
} //4334A269 RET8
std::int32_t COMMAND_FAST unchanged_limit(NativeCommandPhaseReceiver*, std::int32_t incoming) {
    return incoming;
} //433D2AAC
void COMMAND_FAST legit_capture(NativeCommandPhaseReceiver* receiver, void*, Command*) { //4332CDEA
    auto& owner = source<LegitCommandOwner>(receiver);
    owner.capture_view();
}

void COMMAND_FAST viewmodel_capture(NativeCommandPhaseReceiver* receiver, void*,
                                    Command* command) { //433FA91C
    misc::record_viewmodel_command(source<misc::ViewmodelLagState>(receiver), *command);
}

void COMMAND_FAST viewmodel_prepare(NativeCommandPhaseReceiver* receiver, void*, void*,
                                    Command* command, std::uint8_t*) { //433AFBCC
    misc::record_viewmodel_command(source<misc::ViewmodelLagState>(receiver), *command);
}

void COMMAND_FAST tick_shift_prepare(NativeCommandPhaseReceiver* receiver, void* player, void* weapon,
                                  Command* command, std::uint8_t* send) { //433ABC86
    network::tick_shift_owner_phase1(source<network::TickShiftCommandOwner>(receiver), player, weapon,
                                  *command, *send);
}

void COMMAND_FAST performance_prepare(NativeCommandPhaseReceiver* receiver, void*, void*, Command*,
                                      std::uint8_t*) { //433654D5
    source<misc::LowFpsRuntime>(receiver).phase1(receiver->services->input);
}

void COMMAND_FAST entity_prepare(NativeCommandPhaseReceiver* receiver, void* player, void*,
                                 Command* command, std::uint8_t*) { //4334BB84
    process_entity_command_phase1(source<EntityCommandOwner>(receiver), player, *command);
}

void COMMAND_FAST grenade_prepare(NativeCommandPhaseReceiver* receiver, void* player, void*,
                                  Command* command, std::uint8_t*) { //433B7F7E
    source<misc::GrenadeCommandRuntime>(receiver).phase1(player, *command);
}

void COMMAND_FAST movement_prepare(NativeCommandPhaseReceiver* receiver, void*, void*,
                                   Command* command, std::uint8_t*) { //4333F746
    movement_owner_phase1(source<MovementCommandPhaseOwner>(receiver), *command);
}

void COMMAND_FAST prediction_prepare(NativeCommandPhaseReceiver* receiver, void* player, void*,
                                     Command* command, std::array<float, 3>*,
                                     std::uint8_t*) { //433B383E
    prepare_prediction_history(source<PredictionHistoryOwner>(receiver), player, *command);
}

void COMMAND_FAST quick_switch_prepare(NativeCommandPhaseReceiver* receiver, void* player,
                                       void* weapon, Command* command, std::array<float, 3>*,
                                       std::uint8_t* send) { //4341704A
    std::get<misc::QuickSwitchRuntime*>(source<misc::CommandOwner>(receiver))
        ->process_phase2(player, weapon, *command, *send);
}

void COMMAND_FAST tick_shift_preprocess(NativeCommandPhaseReceiver* receiver, void* player,
                                     void* weapon, Command* command, std::array<float, 3>* angles,
                                     std::uint8_t* send) { //433D6A11
    network::tick_shift_owner_phase2(source<network::TickShiftCommandOwner>(receiver), player, weapon,
                                  *command, *send, *angles);
}

void COMMAND_FAST buffered_prepare(NativeCommandPhaseReceiver* receiver, void*, void*, Command*,
                                   std::array<float, 3>*, std::uint8_t*) { //433E4CB6
    prepare_buffered_shot_commands(source<BufferedShotCommandOwner>(receiver));
}

void COMMAND_FAST script_prepare(NativeCommandPhaseReceiver* receiver, void*, void*,
                                 Command* command, std::array<float, 3>* angles,
                                 std::uint8_t* send) { //433E41AD
    source<script::ScriptCommandOwner>(receiver).phase2(*command, *angles, *send);
}

void COMMAND_FAST blockbot_prepare(NativeCommandPhaseReceiver* receiver, void* player, void*,
                                   Command* command, std::array<float, 3>* angles,
                                   std::uint8_t*) { //433B92DC
    auto& owner = std::get<misc::BlockbotCommandOwner>(source<misc::CommandOwner>(receiver));
    MovementAngles incoming{(*angles)[0], (*angles)[1], (*angles)[2]};
    process_blockbot(owner.state, owner.enabled.value != 0, owner.hotkey.value.binding,
                     receiver->services->input, player, *command, incoming, owner.bindings);
    *angles = {incoming.pitch, incoming.yaw, incoming.roll};
}

void COMMAND_FAST rage_prepare(NativeCommandPhaseReceiver* receiver, void* player, void* weapon,
                               Command* command, std::array<float, 3>* angles,
                               std::uint8_t* send) { //43358716
    source<rage::CommandEndpoint>(receiver).phase2(player, weapon, *command, *angles, *send);
}

void COMMAND_FAST rebuy_prepare(NativeCommandPhaseReceiver* receiver, void* player, void*,
                                Command* command, std::array<float, 3>*, std::uint8_t*) { //43407336
    auto* owner = std::get<misc::RebuyFixRuntime*>(source<misc::CommandOwner>(receiver));
    owner->local_player = player;
    owner->update(command->movement);
}

void COMMAND_FAST grenade_preprocess(NativeCommandPhaseReceiver* receiver, void* player,
                                     void* weapon, Command* command, std::array<float, 3>* angles,
                                     std::uint8_t*) { //43383139
    source<misc::GrenadeCommandRuntime>(receiver).phase2(player, weapon, *command, *angles,
                                                         receiver->services->input);
}

void COMMAND_FAST no_fall_prepare(NativeCommandPhaseReceiver* receiver, void* player, void*,
                                  Command* command, std::array<float, 3>*,
                                  std::uint8_t*) { //43401BDB
    source<misc::NoFallDamageRuntime>(receiver).phase2(player, *command);
}

void COMMAND_FAST movement_preprocess(NativeCommandPhaseReceiver* receiver, void* player,
                                      void* weapon, Command* command, std::array<float, 3>* angles,
                                      std::uint8_t* send) { //4334659B
    movement_owner_phase2(source<MovementCommandPhaseOwner>(receiver), player, weapon, *command,
                          *angles, *send);
}

void COMMAND_FAST packet_prepare(NativeCommandPhaseReceiver* receiver, void* player, void* weapon,
                                 Command* command, std::array<float, 3>* angles,
                                 std::uint8_t* send) { //433EA911
    network::process_packet_phase2(source<network::PacketCommandOwner>(receiver), player, weapon,
                                   *command, *angles, *send, receiver->services->scheduler_flags);
}

void COMMAND_FAST anti_aim_prepare(NativeCommandPhaseReceiver* receiver, void* player, void* weapon,
                                   Command* command, std::array<float, 3>* angles,
                                   std::uint8_t* send) { //4339F3BF
    anti_aim::command_owner_phase2(source<anti_aim::CommandOwner>(receiver), player, weapon,
                                   *command, *angles, *send);
}

void COMMAND_FAST tick_shift_frame_prepare(NativeCommandPhaseReceiver* receiver, CommandFrame* frame,
                                        void* player, Command* command) { //433B56ED
    network::tick_shift_owner_phase3(source<network::TickShiftCommandOwner>(receiver), *frame, player,
                                  *command, send_packet(*frame));
}

void COMMAND_FAST automatic_process(NativeCommandPhaseReceiver* receiver, CommandFrame* frame,
                                    void* player, Command* command) { //433EA2AA
    auto& owner =
        std::get<misc::AutomaticWeaponsCommandOwner>(source<misc::CommandOwner>(receiver));
    process_automatic_weapons(owner.state, owner.settings, owner.bindings, *frame, player, *command,
                              receiver->services->input);
}

void COMMAND_FAST tick_shift_process(NativeCommandPhaseReceiver* receiver, CommandFrame* frame,
                                  void* player, Command* command) { //433CE4BC
    network::tick_shift_owner_phase4(source<network::TickShiftCommandOwner>(receiver), *frame, player,
                                  *command, send_packet(*frame));
}

void COMMAND_FAST performance_process(NativeCommandPhaseReceiver* receiver, CommandFrame* frame,
                                      void*, Command*) { //433229CE
    source<misc::LowFpsRuntime>(receiver).phase4(*frame);
}

void COMMAND_FAST feedback_process(NativeCommandPhaseReceiver* receiver, CommandFrame* frame, void*,
                                   Command*) { //4340C927
    rage::process_feedback_command_phase4(source<rage::FeedbackCommandOwner>(receiver), *frame);
}

void COMMAND_FAST script_process(NativeCommandPhaseReceiver* receiver, CommandFrame* frame, void*,
                                 Command* command) { //4334A2C8
    source<script::ScriptCommandOwner>(receiver).phase4(*frame, *command);
}

void COMMAND_FAST knife_process(NativeCommandPhaseReceiver* receiver, CommandFrame* frame,
                                void* player, Command* command) { //433257DA
    auto& owner = std::get<misc::KnifeCommandOwner>(source<misc::CommandOwner>(receiver));
    owner.bindings.process(owner.settings, *frame, *command, player, send_packet(*frame),
                           owner.scheduler_flags);
}

void COMMAND_FAST rage_process(NativeCommandPhaseReceiver* receiver, CommandFrame* frame,
                               void* player, Command* command) { //4334F55F
    source<rage::CommandEndpoint>(receiver).process(*frame, player, *command, send_packet(*frame));
}

void COMMAND_FAST target_mask_process(NativeCommandPhaseReceiver* receiver, CommandFrame* frame,
                                      void*, Command*) { //4340029E
    build_command_target_mask(source<TargetMaskOwner>(receiver), *frame);
}

void COMMAND_FAST penetration_process(NativeCommandPhaseReceiver* receiver, CommandFrame* frame,
                                      void* player, Command*) { //4334B1DA
    auto& owner = source<PenetrationReticleCommandOwner>(receiver);
    auto targeting = owner.bindings.targeting;
    targeting.local_player = player;
    const auto& base = owner.bindings;
    const hud::PenetrationReticleEnvironment context{targeting,
                                                     base.maximum_clients,
                                                     base.ballistics,
                                                     base.target_tick,
                                                     base.predicted_tick,
                                                     base.collision_group,
                                                     base.move_parent_handle,
                                                     base.point_contents,
                                                     base.trace_filter_vtable};
    const auto& origin = frame->origin_dc;
    const auto& direction = frame->direction_1a8;
    const hud::PenetrationReticleFrame snapshot{weapon_pointer(*frame),
                                                player,
                                                {origin[0], origin[1], origin[2]},
                                                {direction[0], direction[1], direction[2]}};
    hud::update_penetration_reticle(owner.state, owner.settings, snapshot, context);
}

void COMMAND_FAST legit_process(NativeCommandPhaseReceiver* receiver, CommandFrame* frame,
                                void* player, Command* command) { //43399A38
    auto& owner = source<LegitCommandOwner>(receiver);
    owner.process(*frame, player, *command);
}

void COMMAND_FAST zeus_process(NativeCommandPhaseReceiver* receiver, CommandFrame* frame,
                               void* player, Command* command) { //433A12BD
    auto& owner = std::get<misc::ZeusCommandOwner>(source<misc::CommandOwner>(receiver));
    process_zeus(owner.state, owner.settings, owner.bindings, *frame, player, *command);
}

void COMMAND_FAST grenade_process(NativeCommandPhaseReceiver* receiver, CommandFrame* frame,
                                  void* player, Command* command) { //4338AD80
    source<misc::GrenadeCommandRuntime>(receiver).phase4(*frame, player, *command,
                                                         receiver->services->input);
}

void COMMAND_FAST screen_overlay_process(NativeCommandPhaseReceiver* receiver, CommandFrame* frame,
                                         void* player, Command* command) { //433361BA
    auto& owner = source<ScreenOverlayCommandOwner>(receiver);
    const auto snapshot = hud::screen_overlay_command_frame(frame);
    owner.publisher.publish(owner.state, owner.settings, snapshot, player, *command, owner.offsets,
                            owner.bindings);
}

void COMMAND_FAST movement_process(NativeCommandPhaseReceiver* receiver, CommandFrame* frame, void*,
                                   Command* command) { //433D1F99
    movement_owner_phase4(source<MovementCommandPhaseOwner>(receiver), *frame, *command);
}

void COMMAND_FAST packet_process(NativeCommandPhaseReceiver* receiver, CommandFrame* frame,
                                 void* player, Command* command) { //433A069B
    network::process_packet_phase4(source<network::PacketCommandOwner>(receiver), *frame, player,
                                   *command, send_packet(*frame),
                                   receiver->services->scheduler_flags);
}

void COMMAND_FAST inventory_process(NativeCommandPhaseReceiver* receiver, CommandFrame* frame,
                                    void* player, Command*) { //43344A6C
    source<skins::CommandRuntime>(receiver).phase4(*frame, player);
}

void COMMAND_FAST anti_aim_process(NativeCommandPhaseReceiver* receiver, CommandFrame* frame,
                                   void* player, Command* command) { //4332D1C6
    anti_aim::command_owner_phase4(source<anti_aim::CommandOwner>(receiver), *frame, player,
                                   *command, send_packet(*frame));
}

void COMMAND_FAST tick_shift_generate(NativeCommandPhaseReceiver* receiver, CommandFrame* frame,
                                   void* player, Command* command) { //4336663F
    network::tick_shift_owner_phase5(source<network::TickShiftCommandOwner>(receiver), *frame, player,
                                  *command, send_packet(*frame));
}

void COMMAND_FAST prediction_finish(NativeCommandPhaseReceiver* receiver, CommandFrame* frame,
                                    void* player, Command* command) { //4339FB7D
    finish_prediction_history(source<PredictionHistoryOwner>(receiver), *frame, player, *command,
                              send_packet(*frame));
}

void COMMAND_FAST feedback_finish(NativeCommandPhaseReceiver* receiver, CommandFrame* frame, void*,
                                  Command*) { //4331E7AC
    rage::process_feedback_command_phase6(source<rage::FeedbackCommandOwner>(receiver), *frame);
}

void COMMAND_FAST entity_finish(NativeCommandPhaseReceiver* receiver, CommandFrame* frame,
                                void* player, Command* command) { //433E2400
    process_entity_command_phase6(source<EntityCommandOwner>(receiver), *frame, player,
                                  weapon_pointer(*frame), *command, send_packet(*frame));
}

void COMMAND_FAST buffered_finish(NativeCommandPhaseReceiver* receiver, CommandFrame* frame,
                                  void* player, Command*) { //43323284
    repair_buffered_shot_commands(source<BufferedShotCommandOwner>(receiver), *frame, player,
                                  send_packet(*frame));
}

void COMMAND_FAST script_finish(NativeCommandPhaseReceiver* receiver, CommandFrame* frame, void*,
                                Command* command) { //4339F05D
    source<script::ScriptCommandOwner>(receiver).phase6(*frame, *command);
}

void COMMAND_FAST rage_finish(NativeCommandPhaseReceiver* receiver, CommandFrame* frame, void*,
                              Command* command) { //4335CBFB
    source<rage::CommandEndpoint>(receiver).phase6(*frame, *command);
}

void COMMAND_FAST shared_esp_finish(NativeCommandPhaseReceiver* receiver, CommandFrame*,
                                    void* player, Command*) { //433E3C96
    update_rejected_player(player, source<SharedEspCommandOwner>(receiver).bindings);
}

void COMMAND_FAST effects_finish(NativeCommandPhaseReceiver* receiver, CommandFrame* frame,
                                 void* player, Command* command) { //4339EC1B
    effects::process_bullet_impacts(source<effects::EffectsCommandOwner>(receiver), *frame, player,
                                    *command);
}

void COMMAND_FAST packet_finish(NativeCommandPhaseReceiver* receiver, CommandFrame* frame,
                                void* player, Command* command) { //433A0EFE
    network::process_packet_phase6(source<network::PacketCommandOwner>(receiver), *frame, player,
                                   *command, send_packet(*frame),
                                   receiver->services->scheduler_flags);
}

void COMMAND_FAST anti_aim_finish(NativeCommandPhaseReceiver* receiver, CommandFrame* frame,
                                  void* player, Command* command) { //433470E4
    anti_aim::command_owner_phase6(source<anti_aim::CommandOwner>(receiver), *frame, player,
                                   *command, send_packet(*frame));
}

void COMMAND_FAST prediction_complete(NativeCommandPhaseReceiver* receiver, CommandFrame* frame,
                                      void* player, Command* command) { //43406B4B
    complete_prediction_history(source<PredictionHistoryOwner>(receiver), *frame, player, *command,
                                send_packet(*frame));
}

void COMMAND_FAST tick_shift_complete(NativeCommandPhaseReceiver* receiver, CommandFrame* frame,
                                   void* player, Command* command) { //43359E37
    network::tick_shift_owner_phase7(source<network::TickShiftCommandOwner>(receiver), *frame, player,
                                  *command, send_packet(*frame));
}

void COMMAND_FAST performance_complete(NativeCommandPhaseReceiver* receiver, CommandFrame* frame,
                                       void*, Command*) { //433B22CB
    source<misc::LowFpsRuntime>(receiver).phase7(*frame, receiver->services->input);
}

void COMMAND_FAST target_mask_complete(NativeCommandPhaseReceiver* receiver, CommandFrame* frame,
                                       void*, Command* command) { //43386E3E
    mark_target_mask_attack(source<TargetMaskOwner>(receiver), *frame, *command);
}

void COMMAND_FAST grenade_complete(NativeCommandPhaseReceiver* receiver, CommandFrame* frame,
                                   void* player, Command*) { //4339C567
    source<misc::GrenadeCommandRuntime>(receiver).phase7(*frame, player);
}

void COMMAND_FAST rage_capture(NativeCommandPhaseReceiver* receiver, CommandFrame* frame, void*,
                               Command*) { //433B76DE
    source<rage::CommandEndpoint>(receiver).capture(*frame);
}

std::int32_t COMMAND_FAST packet_limit(NativeCommandPhaseReceiver* receiver,
                                       std::int32_t limit) { //4340354C
    return network::query_packet_command_limit(source<network::PacketCommandOwner>(receiver),
                                               limit);
}

std::int32_t COMMAND_FAST tick_shift_limit(NativeCommandPhaseReceiver* receiver,
                                        std::int32_t limit) { //4340165F
    return network::cap_tick_shift_commands(
        source<network::TickShiftCommandOwner>(receiver).runtime.network, limit);
}

struct SourceTable {
    CommandOwnerId id;
    NativeCommandPhaseVtable entries;
};
const std::array<SourceTable, 28> source_tables{{
    {static_cast<CommandOwnerId>(0x4346FF8Cu),
     {noop_phase0, noop_phase1, prediction_prepare, noop_frame_phase, noop_frame_phase,
      noop_frame_phase, prediction_finish, prediction_complete, noop_frame_phase, unchanged_limit}},
    {static_cast<CommandOwnerId>(0x4346F244u),
     {noop_phase0, noop_phase1, noop_phase2, noop_frame_phase, automatic_process, noop_frame_phase,
      noop_frame_phase, noop_frame_phase, noop_frame_phase, unchanged_limit}},
    {static_cast<CommandOwnerId>(0x4346FC24u),
     {noop_phase0, noop_phase1, quick_switch_prepare, noop_frame_phase, noop_frame_phase,
      noop_frame_phase, noop_frame_phase, noop_frame_phase, noop_frame_phase, unchanged_limit}},
    {static_cast<CommandOwnerId>(0x4346EDD4u),
     {noop_phase0, tick_shift_prepare, tick_shift_preprocess, tick_shift_frame_prepare, tick_shift_process,
      tick_shift_generate, noop_frame_phase, tick_shift_complete, noop_frame_phase, tick_shift_limit}},
    {static_cast<CommandOwnerId>(0x43472148u),
     {noop_phase0, performance_prepare, noop_phase2, noop_frame_phase, performance_process,
      noop_frame_phase, noop_frame_phase, performance_complete, noop_frame_phase, unchanged_limit}},
    {static_cast<CommandOwnerId>(0x4346EED8u),
     {noop_phase0, noop_phase1, noop_phase2, noop_frame_phase, noop_frame_phase, noop_frame_phase,
      noop_frame_phase, noop_frame_phase, noop_frame_phase, unchanged_limit}},
    {static_cast<CommandOwnerId>(0x4346DC2Cu),
     {noop_phase0, noop_phase1, noop_phase2, noop_frame_phase, feedback_process, noop_frame_phase,
      feedback_finish, noop_frame_phase, noop_frame_phase, unchanged_limit}},
    {static_cast<CommandOwnerId>(0x43470200u),
     {noop_phase0, entity_prepare, noop_phase2, noop_frame_phase, noop_frame_phase,
      noop_frame_phase, entity_finish, noop_frame_phase, noop_frame_phase, unchanged_limit}},
    {static_cast<CommandOwnerId>(0x4346F01Cu),
     {noop_phase0, noop_phase1, buffered_prepare, noop_frame_phase, noop_frame_phase,
      noop_frame_phase, buffered_finish, noop_frame_phase, noop_frame_phase, unchanged_limit}},
    {static_cast<CommandOwnerId>(0x43470834u),
     {noop_phase0, noop_phase1, script_prepare, noop_frame_phase, script_process, noop_frame_phase,
      script_finish, noop_frame_phase, noop_frame_phase, unchanged_limit}},
    {static_cast<CommandOwnerId>(0x43471E50u),
     {noop_phase0, noop_phase1, blockbot_prepare, noop_frame_phase, noop_frame_phase,
      noop_frame_phase, noop_frame_phase, noop_frame_phase, noop_frame_phase, unchanged_limit}},
    {static_cast<CommandOwnerId>(0x43471EB8u),
     {noop_phase0, noop_phase1, noop_phase2, noop_frame_phase, knife_process, noop_frame_phase,
      noop_frame_phase, noop_frame_phase, noop_frame_phase, unchanged_limit}},
    {static_cast<CommandOwnerId>(0x43472A00u),
     {noop_phase0, noop_phase1, rage_prepare, noop_frame_phase, rage_process, noop_frame_phase,
      rage_finish, noop_frame_phase, rage_capture, unchanged_limit}},
    {static_cast<CommandOwnerId>(0x43470478u),
     {noop_phase0, noop_phase1, noop_phase2, noop_frame_phase, target_mask_process,
      noop_frame_phase, noop_frame_phase, target_mask_complete, noop_frame_phase, unchanged_limit}},
    {static_cast<CommandOwnerId>(0x43471F0Cu),
     {noop_phase0, noop_phase1, rebuy_prepare, noop_frame_phase, noop_frame_phase, noop_frame_phase,
      noop_frame_phase, noop_frame_phase, noop_frame_phase, unchanged_limit}},
    {static_cast<CommandOwnerId>(0x4346F798u),
     {noop_phase0, noop_phase1, noop_phase2, noop_frame_phase, penetration_process,
      noop_frame_phase, noop_frame_phase, noop_frame_phase, noop_frame_phase, unchanged_limit}},
    {static_cast<CommandOwnerId>(0x4346F5B0u),
     {legit_capture, noop_phase1, noop_phase2, noop_frame_phase, legit_process, noop_frame_phase,
      noop_frame_phase, noop_frame_phase, noop_frame_phase, unchanged_limit}},
    {static_cast<CommandOwnerId>(0x434701A0u),
     {noop_phase0, noop_phase1, noop_phase2, noop_frame_phase, zeus_process, noop_frame_phase,
      noop_frame_phase, noop_frame_phase, noop_frame_phase, unchanged_limit}},
    {static_cast<CommandOwnerId>(0x4346FE30u),
     {noop_phase0, noop_phase1, noop_phase2, noop_frame_phase, noop_frame_phase, noop_frame_phase,
      shared_esp_finish, noop_frame_phase, noop_frame_phase, unchanged_limit}},
    {static_cast<CommandOwnerId>(0x4346F0ACu),
     {noop_phase0, grenade_prepare, grenade_preprocess, noop_frame_phase, grenade_process,
      noop_frame_phase, noop_frame_phase, grenade_complete, noop_frame_phase, unchanged_limit}},
    {static_cast<CommandOwnerId>(0x4346F048u),
     {viewmodel_capture, viewmodel_prepare, noop_phase2, noop_frame_phase, noop_frame_phase,
      noop_frame_phase, noop_frame_phase, noop_frame_phase, noop_frame_phase, unchanged_limit}},
    {static_cast<CommandOwnerId>(0x4346DCC0u),
     {noop_phase0, noop_phase1, noop_phase2, noop_frame_phase, screen_overlay_process,
      noop_frame_phase, noop_frame_phase, noop_frame_phase, noop_frame_phase, unchanged_limit}},
    {static_cast<CommandOwnerId>(0x43472998u),
     {noop_phase0, noop_phase1, no_fall_prepare, noop_frame_phase, noop_frame_phase,
      noop_frame_phase, noop_frame_phase, noop_frame_phase, noop_frame_phase, unchanged_limit}},
    {static_cast<CommandOwnerId>(0x434700E4u),
     {noop_phase0, noop_phase1, noop_phase2, noop_frame_phase, noop_frame_phase, noop_frame_phase,
      effects_finish, noop_frame_phase, noop_frame_phase, unchanged_limit}},
    {static_cast<CommandOwnerId>(0x43470770u),
     {noop_phase0, movement_prepare, movement_preprocess, noop_frame_phase, movement_process,
      noop_frame_phase, noop_frame_phase, noop_frame_phase, noop_frame_phase, unchanged_limit}},
    {static_cast<CommandOwnerId>(0x43470390u),
     {noop_phase0, noop_phase1, packet_prepare, noop_frame_phase, packet_process, noop_frame_phase,
      packet_finish, noop_frame_phase, noop_frame_phase, packet_limit}},
    {static_cast<CommandOwnerId>(0x4346F1F4u),
     {noop_phase0, noop_phase1, noop_phase2, noop_frame_phase, inventory_process, noop_frame_phase,
      noop_frame_phase, noop_frame_phase, noop_frame_phase, unchanged_limit}},
    {static_cast<CommandOwnerId>(0x4346F2E0u),
     {noop_phase0, noop_phase1, anti_aim_prepare, noop_frame_phase, anti_aim_process,
      noop_frame_phase, anti_aim_finish, noop_frame_phase, noop_frame_phase, unchanged_limit}},
}};
#undef COMMAND_FAST
}
const NativeCommandPhaseVtable& source_command_phase_table(CommandOwnerId id) {
    for (const auto& table : source_tables)
        if (table.id == id)
            return table.entries;
    throw std::invalid_argument("Unknown native command source table");
}
bool is_source_command_phase_table(const NativeCommandPhaseVtable* table) noexcept {
    for (const auto& source : source_tables)
        if (table == &source.entries)
            return true;
    return false;
}
bool command_phase_source_bound(CommandOwnerId id, std::uint32_t phase) {
    if (phase >= 10)
        throw std::invalid_argument("Invalid CreateMove phase");
    (void)source_command_phase_table(id);
    return true;
}
void LegitCommandOwner::capture_view() {
    if (auto* source = std::get_if<Standalone>(&source_))
        legit::capture_legit_view(state, source->engine);
    else
        std::get<legit::CommandSource*>(source_)->capture_view(state);
}
void LegitCommandOwner::clear_allocated_storage() {
    state = legit::RuntimeState{};
    if (auto* source = std::get_if<legit::CommandSource*>(&source_))
        (*source)->clear_allocated_storage();
}
void LegitCommandOwner::process(CommandFrame& frame, void* player, Command& command) {
    if (auto* source = std::get_if<Standalone>(&source_))
        legit::process_legit_command(state, frame, player, command, settings, source->bindings);
    else
        std::get<legit::CommandSource*>(source_)->process(state, settings, frame, player, command);
}
void LegitCommandOwner::reset() {
    if (auto* source = std::get_if<Standalone>(&source_))
        legit::reset_runtime_live(state, source->bindings.times.interval, source->bindings.random);
    else
        std::get<legit::CommandSource*>(source_)->reset(state);
}
void LegitCommandOwner::pending(Command& command, float sample_time) {
    if (auto* source = std::get_if<Standalone>(&source_))
        legit::apply_pending_aim(state, command, sample_time, *source->bindings.times.interval);
    else
        std::get<legit::CommandSource*>(source_)->pending(state, command, sample_time);
}
} //namespace skeet::client
