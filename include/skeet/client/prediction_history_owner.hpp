#pragma once
#include "skeet/network/tick_shift_recharge_receiver.hpp"
#include "skeet/prediction/observer_receiver.hpp"
#include "command_frame.hpp"
#include "restricted_player_command.hpp"
#include "skeet/network/data_update_receiver.hpp"
#include "command_empty_tick_receiver.hpp"
#include <memory>

namespace skeet::animation {
struct LocalPredictionPoseBindings;
}
namespace skeet::client {
struct DeferredPredictionHistoryStorageTag {
    explicit constexpr DeferredPredictionHistoryStorageTag() = default;
};
inline constexpr DeferredPredictionHistoryStorageTag deferred_prediction_history_storage{};
struct PredictionHistoryString {
    const char* begin = nullptr;
    const char* end = nullptr;
    const char* limit = nullptr;
};
std::span<const char* const, 22> prediction_history_sequence_strings() noexcept;
// Canonical storage for the single434759B4 object. The bound arrays are those
// consumed by RestrictedPlayerCommandOwner and tick-shift clones. Sequence
// consumers borrow the same22owned descriptors through typed accessors.
class PredictionHistoryStorage {
    struct StandaloneBuffers;
    std::unique_ptr<StandaloneBuffers> standalone_;

public:
    // Restored4347D094 reads the pointer43475BA4 into full-owner+38. Preserve
    // that live alias rather than caching its current acknowledged number.
    explicit PredictionHistoryStorage(const std::int32_t* acknowledged_command) noexcept;
    // Production4347D010 starts with zeroed storage, registers all seven
    // interfaces, then binds its two genuine allocations and owned strings.
    // This route never constructs or falls back to standalone buffers.
    explicit PredictionHistoryStorage(DeferredPredictionHistoryStorageTag) noexcept;
    // Only the standalone backing is automatic. Production native buffers
    // and strings belong to the actual primary lifecycle destructor.
    ~PredictionHistoryStorage();
    PredictionHistoryStorage(const PredictionHistoryStorage&) = delete;
    PredictionHistoryStorage& operator=(const PredictionHistoryStorage&) = delete;
    RestrictedPlayerCommandOwner& shared() noexcept {
        return view_;
    }
    bool has_standalone_backing() const noexcept {
        return bool(standalone_);
    }
    void initialize_header(const std::int32_t*) noexcept;
    void bind_snapshots(network::TickShiftCommandSnapshot*, std::size_t count) noexcept;
    void bind_prediction_records(RestrictedCommandPrediction0x4C*, std::size_t count) noexcept;
    void bind_sequence_string(std::size_t index, PredictionHistoryString) noexcept;
    const std::int32_t* field_34 = nullptr; // canonical leg_movement payload
    const std::int32_t* acknowledged_command = nullptr;
    std::span<network::TickShiftCommandSnapshot> snapshots;
    std::span<RestrictedCommandPrediction0x4C> prediction_records;
    std::array<PredictionHistoryString, 22> sequence_strings{};
    std::int32_t redirect_from_44 = 0, redirect_to_48 = 0;
    std::array<std::uint32_t, 150> command_tags{}, landing_tags{};

private:
    RestrictedPlayerCommandOwner view_{};
};

struct PredictionHistoryBindings {
    const RestrictedPlayerCommandBindings& command;
    const animation::LocalPredictionPoseBindings& pose;
    const std::int32_t* const& last_outgoing; // live43475B2C pointer cell
    std::ptrdiff_t rejection_byte; // decoded9D920E35
    std::ptrdiff_t restored_angles; // decodedC865F10E
    std::ptrdiff_t absolute_angles; // decoded3B1F6ECC
    lagcomp::SetAbsoluteVector const& set_absolute_angles; // optional434688C8
    engine::MemberFunction<void, std::int32_t> const& invalidate_angles; // optional43468C10
};
// All six secondary interfaces produced by4347D010 refer to this same canonical
// owner. The redirect words are full-owner+44/+48, not the command subobject.
// Snapshot and prediction arrays are borrowed from shared, never duplicated.
struct PredictionHistoryOwner {
    RestrictedPlayerCommandOwner& shared;
    std::int32_t& redirect_from_44;
    std::int32_t& redirect_to_48;
    const PredictionHistoryBindings& bindings;
    network::NativeDataUpdateReceiver data_update_receiver{};
    // Non-owning source linkage, established by the actual lifecycle before
    // registration. This is the same canonical storage, not another history.
    PredictionHistoryStorage* canonical_storage = nullptr;
    network::NativeTickShiftRechargeReceiver recharge_receiver{};
    NativeCommandEmptyTickReceiver empty_tick_receiver{};
    prediction::NativePredictionObserverReceiver prediction_receiver{};
};
// Nullable native owner-slot consumers borrow the actual shared view. No
// independent RestrictedPlayerCommandOwner* cell is maintained.
RestrictedPlayerCommandOwner* prediction_history_shared(PredictionHistoryOwner*) noexcept;
const RestrictedPlayerCommandOwner*
prediction_history_shared(const PredictionHistoryOwner*) noexcept;
network::TickShiftCommandSnapshot* prediction_history_snapshots(PredictionHistoryOwner*) noexcept;
// A nonnull published production owner has its lifecycle's storage linkage.
PredictionHistoryStorage& prediction_history_storage(PredictionHistoryOwner&) noexcept;
const PredictionHistoryStorage& prediction_history_storage(const PredictionHistoryOwner&) noexcept;
const char* prediction_history_sequence_string(const PredictionHistoryOwner&,
                                               std::size_t index) noexcept;

// Complete433CAE9B and4336B342. Snapshot indexing is unsigned modulo150.
bool restore_prediction_snapshot(PredictionHistoryOwner&, void* player, std::int32_t number);
bool copy_prediction_snapshot(PredictionHistoryOwner&, std::int32_t from, std::int32_t to) noexcept;
// Commandtable4346FF8C phases2,6,7 respectively.
void prepare_prediction_history(PredictionHistoryOwner&, void* player, const Command&);
void finish_prediction_history(PredictionHistoryOwner&, CommandFrame&, void* player, Command&,
                               std::uint8_t& send_packet);
void complete_prediction_history(PredictionHistoryOwner&, const CommandFrame&, void* player,
                                 const Command&, std::uint8_t send_packet);
// Complete4337D962, empty-ticktable43470470 slot0. Remaining native arguments
// are unused. A valid player is a native precondition while either mode is on.
void apply_empty_prediction_history(PredictionHistoryOwner&, void* player);
// Complete43397E0D, table43471DD4 slot1 in the separate43475BA8 registry.
void observe_prediction_history(PredictionHistoryOwner&, void* player);
// Complete4332C3C9/4340B612: the canonical0x4C before/after prediction capture.
void begin_prediction_history(PredictionHistoryOwner&, void* player, const Command&);
void capture_prediction_history(PredictionHistoryOwner&, void* player, const Command&);
// Complete433B6972, table434723B8 slot1 in43475784/88. Its slot2 is a real no-op.
void clone_previous_prediction_snapshot(PredictionHistoryOwner&, std::int32_t number) noexcept;
// Complete433F4628, table43470720 slot1 in43475760. A nonzero update
// requires a valid player. Repairs only the canonical cached layer words.
void finish_prediction_data_update(PredictionHistoryOwner&, void* player, std::int32_t update);
} // namespace skeet::client
