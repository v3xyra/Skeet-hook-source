#include "skeet/client/prediction_history_owner.hpp"
#include "skeet/animation/local_prediction_pose.hpp"
#include "skeet/movement/steering_math.hpp"
#include <bit>
#include <cmath>
#include <cstring>

namespace skeet::client {
namespace {
template <class T> T field(const void* object, std::ptrdiff_t offset) noexcept {
    T value;
    std::memcpy(&value, static_cast<const std::byte*>(object) + offset, sizeof value);
    return value;
}
template <class T> void write(void* object, std::ptrdiff_t offset, const T& value) noexcept {
    std::memcpy(static_cast<std::byte*>(object) + offset, &value, sizeof value);
}
bool enabled(const PredictionHistoryOwner& owner) noexcept { // complete4341ACE0
    const auto& b = owner.bindings.command;
    return *b.enabled_43467224 || *b.enabled_4346a740;
}
std::int32_t previous(std::int32_t number) noexcept {
    return std::bit_cast<std::int32_t>(std::uint32_t(number) - 1U);
}
void publish_player_angles(PredictionHistoryOwner& owner, void* player) {
    const auto& b = owner.bindings;
    auto angles = *engine::invoke<const std::array<float, 3>*>(player, 11);
    for (auto& angle : angles)
        angle = normalize_movement_angle(angle);
    *b.command.sent_angles = angles;
    const auto actual = *engine::invoke<const std::array<float, 3>*>(player, 11);
    write(player, b.restored_angles, actual);
}
void apply_sent_angles(PredictionHistoryOwner& owner, void* player) {
    const auto& b = owner.bindings;
    write(player, b.restored_angles, *b.command.sent_angles);
    const auto absolute = field<std::array<float, 3>>(player, b.absolute_angles);
    const auto& sent = *b.command.sent_angles;
    if (sent[0] == absolute[0] && sent[1] == absolute[1] && sent[2] == absolute[2])
        return;
    if (b.invalidate_angles)
        b.invalidate_angles(player, 2);
    // The engine invalidation is allowed to change the shared sent-angle
    // words. Native reloads them before writing the absolute-angle field.
    write(player, b.absolute_angles, *b.command.sent_angles);
}
// Complete433FD134 at the two fixed433F4628 call sites. The scale is exact
// for both8-bit[0,1] and15-bit[0,32767/32768], so the retry arm is not entered.
float quantize_cached_layer(float value, float maximum, float scale) noexcept {
    if (value < 0.0f)
        return 0.0f;
    if (value > maximum)
        return maximum;
    if (std::isnan(value))
        return 0.0f; // FISTP indefinite has a zero low dword.
    const float scaled = value * scale;
    const auto integer = static_cast<std::uint32_t>(std::nearbyint(static_cast<double>(scaled)));
    return static_cast<float>(integer) / scale;
}
}

std::span<const char* const, 22> prediction_history_sequence_strings() noexcept {
    static constexpr std::array<const char*, 22> strings{
        "_moving",        "_crouch", "moving", "crouch", "shield",  "land_", "heavy", "light",
        "reposition_to_", "idle",    "jump",   "knife",  "pistol",  "smg",   "rifle", "shotgun",
        "sniper",         "heavy",   "c4",     "pistol", "grenade", "knife"};
    return strings;
}
struct PredictionHistoryStorage::StandaloneBuffers {
    std::array<network::TickShiftCommandSnapshot, 150> snapshots{};
    std::array<RestrictedCommandPrediction0x4C, 150> prediction_records{};
};
PredictionHistoryStorage::PredictionHistoryStorage(DeferredPredictionHistoryStorageTag) noexcept {
    view_.command_tags = command_tags.data();
    view_.landing_tags = landing_tags.data();
}
PredictionHistoryStorage::~PredictionHistoryStorage() = default;
void PredictionHistoryStorage::initialize_header(const std::int32_t* current_ack) noexcept {
    field_34 = nullptr;
    acknowledged_command = current_ack;
    view_.last_processed_command = current_ack;
}
void PredictionHistoryStorage::bind_snapshots(network::TickShiftCommandSnapshot* first,
                                              std::size_t count) noexcept {
    snapshots = first ? std::span(first, count) : std::span<network::TickShiftCommandSnapshot>{};
    view_.snapshots = first;
}
void PredictionHistoryStorage::bind_prediction_records(RestrictedCommandPrediction0x4C* first,
                                                       std::size_t count) noexcept {
    prediction_records =
        first ? std::span(first, count) : std::span<RestrictedCommandPrediction0x4C>{};
    view_.prediction_records = first;
}
void PredictionHistoryStorage::bind_sequence_string(std::size_t index,
                                                    PredictionHistoryString text) noexcept {
    sequence_strings[index] = text;
    switch (index) {
    case 0:
        view_.moving_modifier = text.begin;
        break;
    case 1:
        view_.crouching_modifier = text.begin;
        break;
    case 3:
        view_.layer_modifier = text.begin;
        break;
    case 8:
        view_.idle_activity = text.begin;
        break;
    case 9:
        view_.idle_modifier = text.begin;
        break;
    case 10:
        view_.jump_activity = text.begin;
        break;
    default:
        break;
    }
}
PredictionHistoryStorage::PredictionHistoryStorage(const std::int32_t* current_ack) noexcept
    : PredictionHistoryStorage(deferred_prediction_history_storage) {
    // Explicit semantic/fixture route. The lifecycle constructor below uses
    // the deferred route and the actual two native heap allocations instead.
    standalone_ = std::make_unique<StandaloneBuffers>();
    initialize_header(current_ack);
    bind_snapshots(standalone_->snapshots.data(), 150);
    bind_prediction_records(standalone_->prediction_records.data(), 150);
    for (auto& snapshot : snapshots)
        animation::initialize_animation_state(reinterpret_cast<std::byte*>(&snapshot) + 0x1F4);
    redirect_from_44 = redirect_to_48 = -1;
    const auto strings = prediction_history_sequence_strings();
    for (std::size_t index = 0; index < strings.size(); ++index)
        bind_sequence_string(index, {strings[index], nullptr, nullptr});
}

RestrictedPlayerCommandOwner* prediction_history_shared(PredictionHistoryOwner* owner) noexcept {
    return owner ? &owner->shared : nullptr;
}
const RestrictedPlayerCommandOwner*
prediction_history_shared(const PredictionHistoryOwner* owner) noexcept {
    return owner ? &owner->shared : nullptr;
}
network::TickShiftCommandSnapshot*
prediction_history_snapshots(PredictionHistoryOwner* owner) noexcept {
    return owner ? owner->shared.snapshots : nullptr;
}
PredictionHistoryStorage& prediction_history_storage(PredictionHistoryOwner& owner) noexcept {
    return *owner.canonical_storage;
}
const PredictionHistoryStorage&
prediction_history_storage(const PredictionHistoryOwner& owner) noexcept {
    return *owner.canonical_storage;
}
const char* prediction_history_sequence_string(const PredictionHistoryOwner& owner,
                                               std::size_t index) noexcept {
    return prediction_history_storage(owner).sequence_strings[index].begin;
}

bool restore_prediction_snapshot(PredictionHistoryOwner& owner, void* player, std::int32_t number) {
    if (number == owner.redirect_from_44)
        number = owner.redirect_to_48;
    const auto& snapshot = owner.shared.snapshots[std::uint32_t(number) % 150U];
    if (snapshot.command_number != number)
        return false;
    const auto saved = field<lagcomp::AnimationState>(&snapshot, 0x14);
    animation::restore_entity_animation(saved, player, owner.bindings.command.offsets.animation);
    if (owner.bindings.set_absolute_angles) {
        const std::array<float, 3> angles{0.0f, field<float>(&snapshot, 0x1E4), 0.0f};
        owner.bindings.set_absolute_angles(player, angles);
    }
    return true;
}
bool copy_prediction_snapshot(PredictionHistoryOwner& owner, std::int32_t from,
                              std::int32_t to) noexcept {
    const auto& source = owner.shared.snapshots[std::uint32_t(from) % 150U];
    if (source.command_number != from)
        return false;
    auto& destination = owner.shared.snapshots[std::uint32_t(to) % 150U];
    std::memmove(&destination, &source, sizeof destination);
    destination.command_number = to;
    return true;
}
void prepare_prediction_history(PredictionHistoryOwner& owner, void* player,
                                const Command& command) {
    if (enabled(owner))
        restore_prediction_snapshot(owner, player, previous(command.number));
}
void finish_prediction_history(PredictionHistoryOwner& owner, CommandFrame& frame, void* player,
                               Command& command, std::uint8_t& send_packet) {
    process_restricted_player_command(&owner.shared, player, &command, &send_packet, true,
                                      owner.bindings.command);
    animation::publish_local_prediction_pose(frame, player, command, send_packet,
                                             owner.bindings.pose);
    if (!enabled(owner))
        return;
    if (!send_packet)
        restore_prediction_snapshot(owner, player, *owner.bindings.last_outgoing);
    publish_player_angles(owner, player);
}
void complete_prediction_history(PredictionHistoryOwner& owner, const CommandFrame& frame,
                                 void* player, const Command& command, std::uint8_t send_packet) {
    if (!send_packet || !(frame.flags & 0x10000000U) || !enabled(owner))
        return;
    owner.redirect_from_44 = command.number;
    owner.redirect_to_48 = *owner.bindings.last_outgoing;
    if (restore_prediction_snapshot(owner, player, *owner.bindings.last_outgoing))
        publish_player_angles(owner, player);
}
void apply_empty_prediction_history(PredictionHistoryOwner& owner, void* player) {
    if (enabled(owner))
        apply_sent_angles(owner, player);
}
void observe_prediction_history(PredictionHistoryOwner& owner, void* player) {
    if (enabled(owner) && player && !field<std::uint8_t>(player, owner.bindings.rejection_byte))
        apply_sent_angles(owner, player);
}
void begin_prediction_history(PredictionHistoryOwner& owner, void* player, const Command& command) {
    if (enabled(owner))
        owner.shared.prediction_records[std::uint32_t(command.number) % 150U].tick_base =
            field<std::int32_t>(player, owner.bindings.command.offsets.tick_base);
}
void capture_prediction_history(PredictionHistoryOwner& owner, void* player,
                                const Command& command) {
    const auto slot = std::uint32_t(command.number) % 150U;
    auto& record = owner.shared.prediction_records[slot];
    if (!enabled(owner)) {
        record.command_number = -1;
        return;
    }
    const auto& offsets = owner.bindings.command.offsets;
    record.command_number = command.number;
    record.field_04 =
        (record.field_04 & 0xFFFFFF00U) |
        std::uint32_t(owner.shared.command_tags[slot] == std::uint32_t(command.number));
    record.origin = field<std::array<float, 3>>(player, offsets.field_36375c23);
    record.velocity = field<std::array<float, 3>>(player, offsets.field_2a671b42);
    record.field_3309c0cf = field<float>(player, offsets.field_3309c0cf);
    record.field_fb7e3404 = field<float>(player, offsets.animation.field_fb7e3404);
    record.flags = field<std::uint32_t>(player, offsets.field_e2fbe695);
    const auto byte = field<std::uint8_t>(player, offsets.field_ac7f2913);
    write(&record, 0x24, std::uint32_t(byte));
    record.field_7090d8dd = field<std::uint8_t>(player, offsets.field_7090d8dd);
    record.field_cacaeecc = field<std::uint8_t>(player, offsets.field_cacaeecc);
    record.field_a15c3105 = field<std::uint8_t>(player, offsets.field_a15c3105);
    record.field_ae838771 = field<float>(player, offsets.field_ae838771);
    record.field_d4248210 = field<std::uint32_t>(player, offsets.field_d4248210);
    record.field_a3abbb09 = field<std::uint32_t>(player, offsets.field_a3abbb09);
    record.field_7501cb91 = field<std::uint8_t>(player, offsets.field_7501cb91);
}
void clone_previous_prediction_snapshot(PredictionHistoryOwner& owner,
                                        std::int32_t number) noexcept {
    copy_prediction_snapshot(owner, previous(number), number);
}
void finish_prediction_data_update(PredictionHistoryOwner& owner, void* player,
                                   std::int32_t update) {
    if (!update || field<std::uint8_t>(player, owner.bindings.rejection_byte) || !enabled(owner))
        return;
    const auto index = engine::invoke<std::uint32_t>(static_cast<std::byte*>(player) + 8, 10);
    auto& cache = owner.bindings.command.players.get(index)->native;
    bool changed = false;
    for (std::size_t word = 0; word < 91; ++word) {
        if (field<std::uint32_t>(&cache, 0x2FC + word * 4) !=
            field<std::uint32_t>(&cache, 0x468 + word * 4)) {
            changed = true;
            break;
        }
    }
    if (!changed)
        return;
    const auto number = *owner.shared.last_processed_command;
    const auto& snapshot = owner.shared.snapshots[std::uint32_t(number) % 150U];
    if (snapshot.command_number != number)
        return;
    for (std::size_t layer = 0; layer < 13; ++layer) {
        const auto source = 0x18 + layer * 0x1C;
        const auto destination = 0x468 + layer * 0x1C;
        const auto weight = field<float>(&snapshot, source + 12);
        const auto cycle = field<float>(&snapshot, source + 24);
        const float quantized_weight = quantize_cached_layer(weight, 1.0f, 255.0f);
        const float quantized_cycle = quantize_cached_layer(cycle, 0.999969482421875f, 32768.0f);
        if (field<std::uint32_t>(&snapshot, source + 4) !=
                field<std::uint32_t>(&cache, destination + 4) ||
            field<float>(&snapshot, source + 20) != field<float>(&cache, destination + 20) ||
            field<float>(&cache, destination + 12) != quantized_weight ||
            field<float>(&cache, destination + 24) != quantized_cycle)
            continue;
        write(&cache, destination + 12, field<std::uint32_t>(&snapshot, source + 12));
        write(&cache, destination + 24, field<std::uint32_t>(&snapshot, source + 24));
    }
}
} // namespace skeet::client
