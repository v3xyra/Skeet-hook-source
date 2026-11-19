#pragma once
#include "skeet/network/tick_shift_recharge_receiver.hpp"
#include "skeet/prediction/observer_receiver.hpp"
#include "command_frame.hpp"
#include "skeet/prediction/history.hpp"
#include "skeet/network/data_update_receiver.hpp"
#include "skeet/engine/startup_convars.hpp"
#include "skeet/animation/entity_update.hpp"
#include <functional>
#include <span>

namespace skeet::prediction {
struct ObserverRegistry;
struct PlayerCommandContext;
}
namespace skeet::animation {
struct AnimationAngleHistory;
}
namespace skeet::legit {
struct RecoilDecayBindings;
}
namespace skeet::misc {
struct NativeText;
}
namespace skeet::client {
struct RestrictedPlayerCommandOwner;
struct PredictionHistoryOwner;
class PlayerStateCache;
// Actual CFDA2 stack argument, not a command number. Native reloads count
// after every item; the second word in each entry is not read by this owner.
struct EntityPredictionEntry {
    std::int32_t entity_index;
    std::uint32_t field_04;
};
struct EntityPredictionEntities {
    std::array<std::byte, 0x44> prefix;
    std::array<EntityPredictionEntry, 2048> entries;
    std::int32_t count;
};
static_assert(offsetof(EntityPredictionEntities, entries) == 0x44);
static_assert(offsetof(EntityPredictionEntities, count) == 0x4044);
struct RevolverCommandPrediction {
    std::int32_t command_number;
    std::uint32_t weapon_state;
    float postpone_fire;
};
static_assert(sizeof(RevolverCommandPrediction) == 0xC);
struct EntityModelCategoryNode {
    std::uint32_t model_hash, category;
    EntityModelCategoryNode* next;
};
struct EntityModelCategories {
    // One extra entry is the native end marker at buckets[bucket_count].
    std::span<EntityModelCategoryNode* const> buckets;
};
struct EntityDataUpdateSnapshot {
    std::uint32_t field_7f5ed2d0; //full owner+40
    std::array<std::uint32_t, 3> recoil_angles; //+44..4C, raw float words
    std::uint32_t scaled_recoil; //+50
    std::int32_t tick_base, health; //+54/+58
    std::uint32_t field_47a6f50c; //+5C
};
struct EntityAngleConfirmation {
    std::uint32_t index, count; //43479794/43479798
    std::array<std::uint8_t, 3> values; //4347979C..9E
};
static_assert(offsetof(EntityAngleConfirmation, values) == 8);
static_assert(sizeof(EntityDataUpdateSnapshot) == 0x20);
static_assert(offsetof(EntityDataUpdateSnapshot, tick_base) == 0x14);
struct EntityCommandState {
    EntityDataUpdateSnapshot data_update; //one canonical full owner+40..5C snapshot
    std::int32_t last_choked_shot = 0; // full owner+60
    float revolver_delay = 0.23125f; // full owner+64, constructor3E6CCCCD
    RevolverCommandPrediction* revolver; // full owner+68, same150-entry producer
    prediction::CommandHistoryEntry* history; // full owner+6C, canonical history
    std::uint32_t prediction_stamp = 0; // full owner+70, copied as raw time bits
    std::uint32_t data_update_flags = 0; //full owner+74
    const EntityModelCategories* model_categories; // full owner+78/+7C table
};
static_assert(offsetof(EntityCommandState, last_choked_shot) == 0x20);
static_assert(offsetof(EntityCommandState, revolver_delay) == 0x24);
#if UINTPTR_MAX == UINT32_MAX
static_assert(offsetof(EntityCommandState, revolver) == 0x28);
static_assert(offsetof(EntityCommandState, history) == 0x2C);
static_assert(offsetof(EntityCommandState, prediction_stamp) == 0x30);
static_assert(offsetof(EntityCommandState, data_update_flags) == 0x34);
#endif
struct EntityCommandStorage {
    std::array<RevolverCommandPrediction, 150> revolver;
    std::array<prediction::CommandHistoryEntry, 150> history;
};
static_assert(sizeof(std::array<RevolverCommandPrediction, 150>) == 0x708);
static_assert(sizeof(std::array<prediction::CommandHistoryEntry, 150>) == 0x5460);
// Constructor4347F9CA..4347FA24 buffer initialization. Consumers bind views
// of these same arrays; the canonical producer never maintains a mirror.
void initialize_entity_revolver_storage(RevolverCommandPrediction*) noexcept;
void initialize_entity_prediction_storage(prediction::CommandHistoryEntry*) noexcept;
void initialize_entity_command_storage(EntityCommandStorage&) noexcept;
void bind_entity_command_storage(EntityCommandState&, EntityCommandStorage&) noexcept;
struct EntityCommandOffsets {
    std::ptrdiff_t prediction_context; //5DF1A4BC, last command is this+68
    std::ptrdiff_t prediction_angles; //3B1F6ECC, vector
    std::ptrdiff_t tick_base; //3D10B94C
    std::ptrdiff_t vector_5ae32976, vector_1bae1168, vector_48442721;
    std::ptrdiff_t view_offset; //C246315E
    std::ptrdiff_t scalar_2f33c681, field_b5a66556, field_fb7e3404, vector_0317eede;
    std::ptrdiff_t active_weapon; //CD812654
    std::ptrdiff_t weapon_definition, postpone_fire; //5C9F0FBB,43D0EFC4
    std::ptrdiff_t rejected_9d920e35, rejected_cde290d2, team, network_origin;
    std::ptrdiff_t move_type, owner_handle, ground_flags; //7501CB91,8D1BD8D0,E2FBE695
    std::ptrdiff_t activity_8dbc9874, activity_68140cd8, collision_d5fd2e2c;
    std::ptrdiff_t player_flags_38ca5228, state_043fa575, state_334b460c,
        interpolated_origin_6610746e;
    std::ptrdiff_t field_66146802, field_00a5bd68, field_6b1a59b5, field_50f9ccc8;
    std::ptrdiff_t shots_fired_8cfc633a, field_7f5ed2d0, field_bcb263a5;
    std::ptrdiff_t weapon_state_9e2078a1, weapon_ammunition_ca5ec698, weapon_field_0176a381;
    std::ptrdiff_t weapon_field_6777c9ce, weapon_field_d2563274, weapon_field_15335aac;
    std::ptrdiff_t weapon_field_4013fc03, weapon_field_e865a877, weapon_field_3b93667b;
    std::ptrdiff_t weapon_viewmodel_index_c6c78c82;
    std::ptrdiff_t player_field_6e9852d6, player_field_432b1f5b, player_field_0d560fac;
    std::ptrdiff_t view_models_71dcee42, observer_mode_2f4e97c4, observer_target_3b704b41,
        viewmodel_695ff325;
    std::ptrdiff_t field_47a6f50c;
    std::ptrdiff_t weapon_field_01aa449d, viewmodel_weapon_a116369a, viewmodel_sequence_a25d8271;
};
using RestorePredictionFrame = engine::MemberFunction<void, std::int32_t, std::int32_t>;
class EntityAnimationAngleBindings {
public:
    EntityAnimationAngleBindings(animation::AnimationAngleHistory* standalone) noexcept
        : standalone_(standalone) {}
    EntityAnimationAngleBindings(std::uint8_t& active, const std::int32_t* const& frame,
                                 std::array<animation::AnimationAngleSample, 2>& samples) noexcept
        : active_(&active), frame_(&frame), samples_(&samples) {}
    void set_active(bool) const noexcept;
    bool select(std::int32_t frame, RageVector&) const noexcept;

private:
    animation::AnimationAngleHistory* standalone_ = nullptr;
    std::uint8_t* active_ = nullptr;
    const std::int32_t* const* frame_ = nullptr;
    std::array<animation::AnimationAngleSample, 2>* samples_ = nullptr;
};
class EntityConnectionNotification {
public:
    using Entry = void (*)();
    EntityConnectionNotification(Entry value) noexcept : value_(value) {}
    EntityConnectionNotification(std::reference_wrapper<Entry> cell) noexcept
        : value_(&cell.get()) {}
    Entry get() const noexcept {
        if (const auto* cell = std::get_if<Entry const*>(&value_))
            return **cell;
        return std::get<Entry>(value_);
    }
    explicit operator bool() const noexcept {
        return get() != nullptr;
    }
    void operator()() const {
        get()();
    }

private:
    std::variant<Entry, Entry const*> value_;
};
struct EntityCommandBindings {
    void* engine_client; //full owner+34; view18/19 and command query205
    void* prediction; //full owner+3C; actual Update slot3
    void* entity_list; //4346A890
    void* input; //4346A894; actual command ring+F0
    void* model_info; //4346A614; model-name slot3
    engine::ConVarBinding teammates_are_enemies; //live43468CAC XOR D8ED8A94, ConVar
    engine::ConVarBinding interpolation_enabled; //live4346A850 XOR478720C2, ConVar
    engine::ConVarBinding recoil_scale_convar; //live4346794C XORC429E1BF, ConVar
    const std::uint8_t* shot_recording_enabled; //full owner+38 ->43467224
    const std::int32_t* startup_guard_4347bdac;
    const std::int32_t* startup_guard_4347bd78;
    std::uint8_t* connection_checked; //434758B0
    std::uint8_t* deferred_connection; //43475BD2
    EntityConnectionNotification connection_notification; //live optional43468BF0
    const std::int32_t* const& update_frame; //live pointer434758D8
    const std::int32_t* const& old_acknowledged; //live pointer43475794
    const std::int32_t* last_outgoing; //43475B2C
    const std::int32_t* choked_commands; //434757DC
    const std::int32_t* process_milliseconds; //43468290
    void* engine_globals; //43467B04; maxclients+18,receiveinterval+20,tick+28
    const float* prediction_time; //4346A334
    const float* interval; //4346A338
    float* simulation_time; //43467EB8, saved/restored by short-gap correction
    const legit::RecoilDecayBindings* recoil_decay; //three actualConVars/CRTexpf
    std::array<float, 3>* sent_angles; //4347BD30
    RestorePredictionFrame restore_prediction_frame; //actual optional4346D660
    engine::MemberFunction<std::int32_t, std::int32_t> sequence_activity; //actual optional4346A364
    const std::int32_t* data_update_backup_mode; //4347C9EC
    std::int32_t* data_update_reset_marker; //43467114
    std::uint32_t* scheduler_flags; //434759C0
    void* const* physics_class; //43467478; actual client-class identity
    prediction::ObserverRegistry* observers; //canonical43475AB0/AB4 registry
    PredictionHistoryOwner* const& shared_animation_owner; //live canonical434759B4 source owner
    PlayerStateCache* players; //canonical433B8A2B cache
    EntityAnimationAngleBindings angle_history; //canonical434759DC/E8,active43468D7C
    EntityAngleConfirmation* angle_confirmation; //canonical43479794/98/9C queue
    std::span<const std::uint32_t, 3> weapon_classification; //43474580
    EntityCommandOffsets offsets;
};
struct EntityCommandOwner {
    EntityCommandState& state;
    const EntityCommandBindings& bindings;
    network::NativeDataUpdateReceiver data_update_receiver{};
    network::NativeTickShiftRechargeReceiver recharge_receiver{};
    prediction::NativePredictionObserverReceiver prediction_receiver{};
    // Constructed by the actual lifecycle owner, after its eight NativeTexts.
    // This source view never interprets native owner offsets as C++ members.
    std::span<const misc::NativeText> prediction_names{};
};
// Typed carries from43403836..434038F2, shared by the concrete correction
// and finalization cuts. current_record is selected afterward at43404CCF.
struct EntityDataUpdateWork {
    std::int32_t acknowledged_command, acknowledged_delta;
    std::int32_t captured_tick_base, current_tick_base;
    std::uint32_t original_recoil_pitch;
    bool field_7f5ed2d0_changed;
    prediction::CommandHistoryEntry* current_record = nullptr;
};
// Full4334BB84. Quantization repair and prediction Update use the same shared
// command/revolver history as the producer; no second history is allocated.
void process_entity_command_phase1(EntityCommandOwner&, void* player, Command&);
// Full433E2400: remember choked classified shots and invalidate prediction on
// a strict overlap with an eligible player's collision box.
void process_entity_command_phase6(EntityCommandOwner&, const CommandFrame&, void* player,
                                   void* weapon, const Command&, std::uint8_t send_packet);
// Full433CFDA2, typed pointer ABI; ignores its native receiver.
void process_entity_prediction_entities(EntityCommandOwner&, const EntityPredictionEntities&);
// Full433BFDDB, the distinct43475784 slot1 observer. Number is ignored.
void observe_entity_command_recharge(EntityCommandOwner&, std::int32_t command_number) noexcept;
// Actual43475AB0 source observer endpoints, receiver full owner+10.
void begin_entity_prediction(EntityCommandOwner&, void* player, const Command&); //4332BBA4
void finish_entity_prediction(EntityCommandOwner&, void* player, const Command&); //43382703
void reject_entity_prediction(EntityCommandOwner&, void* player,
                              const prediction::PlayerCommandContext&); //433B265C
// Shared concrete field repair in BB84/BBA4/B265C. Includes the native
// tick/flag/time gate, vector tolerances and fixed14-bit scalar quantizer.
void repair_entity_prediction_fields(EntityCommandOwner&, void* player,
                                     const prediction::CommandHistoryState&);
float quantize_entity_prediction_scalar(float) noexcept; //433FD134 fixed14bits,range0..100
// Shared canonical history producer tail in82703/B265C; writes the existing
//150-entry0x90 ring and retains its native per-slot first-capture fields.
void capture_entity_prediction_history(EntityCommandOwner&, void* player,
                                       std::int32_t command_number);
// Concrete43342DD7 indexed viewmodel lookup with live observer-target recursion.
void* entity_command_view_model(EntityCommandOwner&, void* player, std::int32_t index);
// Data-update table4346F814, receiver full owner+18. This is the distinct
//43475760 observer family; capture takes its snapshot before other updates.
void process_entity_data_update_capture(EntityCommandOwner&, void* player,
                                        std::int32_t update); //433D67DF
void process_entity_data_update_finish(EntityCommandOwner&, void* player,
                                       std::int32_t update); //43403681
// Shared reset tail used by43388375 and43403681; only the original ring tags,
// recoil snapshot, last shot and global reset marker are overwritten.
void reset_entity_prediction_history(EntityCommandOwner&) noexcept;
// Concrete inlined43403681 stages, split into readable source units.
std::uint32_t entity_data_update_difference_flags(EntityCommandOwner&, void* player,
                                                  const prediction::CommandHistoryState&,
                                                  std::int32_t command_number,
                                                  bool compare_angles = true);
float entity_data_update_quantized_angle(float) noexcept; //full4337F9D3
void reconcile_entity_data_update_ack(EntityCommandOwner&, void* player,
                                      const EntityDataUpdateWork&);
void finish_entity_data_update_tail(EntityCommandOwner&, void* player, const EntityDataUpdateWork&);
} //namespace skeet::client
