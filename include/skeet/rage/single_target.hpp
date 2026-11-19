#pragma once
#include "acquisition.hpp"
#include "preamble.hpp"
#include "record_candidates.hpp"
#include "body_aim_probe.hpp"
#include "skeet/lagcomp/record_selection.hpp"
#include "skeet/players/state.hpp"

namespace skeet::rage {
struct SingleTargetSelection {
    const TargetBindings& targeting;
    std::int32_t maximum_clients;
    RageVector origin, forward;
    float minimum_cosine;
};
// Complete433EC805. Eligible engine entities are scored at collision-max-Z-8;
// equality retains the earlier entity, and the initial cosine is exclusive.
void* select_single_target(const SingleTargetSelection&);

struct TargetPreparationOffsets {
    std::ptrdiff_t pose_parameters; //1CAAF0A8
    std::ptrdiff_t animation_state; //3D52A1A7
    std::ptrdiff_t studio_header; //433FD30D's decoded wrapper field
    std::ptrdiff_t hitbox_set; //C2B6FB10
    std::ptrdiff_t bone_cache; //17978090, pointer to0x30-byte matrices
    std::ptrdiff_t velocity; //36375C23
    std::ptrdiff_t animation_layers; //B538BBA5
};
struct TargetPreparationBindings {
    const TargetBindings& targeting;
    const GlobalSettings& global;
    const AimbotSettings& weapon_settings;
    InputBitmap input;
    PreambleState& state;
    std::int32_t remembered_target_handle; //shared AA target4346DB1C
    const void* accuracy_convar; //decoded43468EA4
    TargetPreparationOffsets offsets;
};
// Shared native multipoint-scale calculation. The action consumer uses this
// stage directly after its own settings/weapon getter sequence.
float acquisition_point_scale(const client::CommandFrame&, void* target,
                              const AcquisitionTargetSettings&, const TargetPreparationBindings&);
// Complete433E63FD, including newest-three traversal, the0x200 stop marker,
// skipped bit8 records, wrapped yaw difference and strict60-degree threshold.
bool target_history_yaw_changed(void* target, const client::PlayerState&,
                                std::ptrdiff_t animation_state_offset) noexcept;
// Shared503EE..50E12 /51E2D..528CC. Mutates the actual ten-word settings and
// player counter, using engine getters for weapon factors, health and origin.
void prepare_acquisition_target_settings(const client::CommandFrame&, void* target,
                                         client::PlayerState&, std::uint32_t entity_index,
                                         const players::Adjustments&, AcquisitionTargetSettings&,
                                         const TargetPreparationBindings&);
struct AcquisitionTargetTicks {
    std::vector<std::int32_t> values;
    std::uint32_t descriptor_flags = 0;
};
// Shared5154B..51824 /54011..5430E. A nonempty priority list receives flag4
// and descending native qsort order. Fallbacks retain flag0 and may set DD.
// Accuracy mode is the native valid0..4 configuration index.
AcquisitionTargetTicks acquisition_target_ticks(const client::CommandFrame&, void* target,
                                                client::PlayerState&, AcquisitionTargetSettings&,
                                                const TargetPreparationBindings&,
                                                const RecordCandidateBindings&);
struct TargetPredictionBindings {
    const BodyAimProbeContext& trace;
    const lagcomp::RecordSelectionContext& record_selection;
    void* engine_trace; //actual43468304, TraceRay slot5
    std::ptrdiff_t local_maximum_speed; //3EB71843
    std::uint32_t scheduler_flags; //434759C0
    // Post-firing56672 branch reads the active command. The earlier shared
    // acquisition copies retain their separate frame+24/+28 input source.
    const client::Command* command = nullptr;
};
// Shared50E1E..51547 /538BF..54011. Copies the newest complete record,
// refreshes its actual attachments, traces present/future damage and raises D0
// only when the projected shot-count improves under the original tick-shift gates.
void refine_acquisition_minimum_damage(const client::CommandFrame&, void* target,
                                       client::PlayerState&, AcquisitionTargetSettings&,
                                       const TargetPredictionBindings&);
// Production overload:43327659 refreshes live studio/attachment state through
// the required raw runtime. The five-argument fixture projection stays separate.
void refine_acquisition_minimum_damage(const client::CommandFrame&, void* target,
                                       client::PlayerState&, AcquisitionTargetSettings&,
                                       const TargetPredictionBindings&,
                                       const animation::BoneRuntime&);
struct SingleTargetBindings {
    const TargetPreparationBindings& preparation;
    const TargetPredictionBindings& prediction;
    const RecordCandidateBindings& record_candidates;
    const AcquisitionRecordBindings& records;
    const players::State& adjustments;
    std::int32_t maximum_clients;
    engine::JobDispatcher& jobs;
};
// Complete single-target acquisition5034F..51C32 and shared5473E..547C9.
// False is the native empty-selection jump54908, which bypasses the job drain
// and per-player cleanup. Successful selection uses canonical scanner storage.
bool acquire_single_target(PreambleState&, PreambleResult&, AcquisitionStorage&,
                           const SingleTargetBindings&);
} // namespace skeet::rage
