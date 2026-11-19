#include "skeet/client/entity_command_owner.hpp"
#include "skeet/client/startup_reset.hpp"
#include "skeet/client/verified_command.hpp"
#include "skeet/events/game_events.hpp"
#include "skeet/network/packet_control.hpp"
#include "skeet/prediction/observer_registry.hpp"
#include <bit>
#include <cmath>
#include <cstring>
#include <emmintrin.h>

namespace skeet::prediction {
CommandHistoryView
CommandHistoryView::from_owner(const client::EntityCommandOwner* owner) noexcept {
    CommandHistoryView result;
    result.owner_present_ = owner != nullptr;
    if (owner)
        result.entries_ = owner->state.history;
    return result;
}
}

namespace skeet::client {
void EntityAnimationAngleBindings::set_active(bool value) const noexcept {
    if (active_)
        *active_ = std::uint8_t(value);
    else
        standalone_->active = value;
}
bool EntityAnimationAngleBindings::select(std::int32_t frame, RageVector& angles) const noexcept {
    if (active_)
        return animation::select_animation_angles(
            animation::AnimationAngleHistoryView(*active_, *frame_, *samples_), frame, angles);
    return animation::select_animation_angles(*standalone_, frame, angles);
}
namespace {
template <class T> T read(const void* object, std::ptrdiff_t offset = 0) noexcept {
    T value;
    std::memcpy(&value, static_cast<const std::byte*>(object) + offset, sizeof value);
    return value;
}
template <class T> void write(void* object, std::ptrdiff_t offset, const T& value) noexcept {
    std::memcpy(static_cast<std::byte*>(object) + offset, &value, sizeof value);
}
using Vector = std::array<float, 3>;
std::int32_t previous_number(std::int32_t number) noexcept {
    return std::bit_cast<std::int32_t>(std::uint32_t(number) - 1u);
}
// Fixed433FD134 inputs ECX=14, low=0, high=100. Keep its scale retry and
// SSE conversion: a downward-rounded scale can avoid the retry entirely.
float quantized_prediction_scalar(float value) noexcept {
    if (value < 0.0f)
        return 0.0f;
    if (value > 100.0f)
        return 100.0f;
    const auto ratio = _mm_cvtpd_ps(_mm_div_sd(_mm_set_sd(16383.0), _mm_set_sd(100.0)));
    const auto initial = _mm_cvtss_f32(ratio);
    auto scale = initial;
    if (static_cast<double>(scale) * 100.0 > 16383.0) {
        constexpr float factors[]{0.9998999834060669f, 0.9900000095367432f, 0.8999999761581421f,
                                  0.800000011920929f, 0.699999988079071f};
        for (const auto factor : factors) {
            scale = _mm_cvtss_f32(_mm_mul_ss(_mm_set_ss(factor), _mm_set_ss(initial)));
            if (static_cast<double>(scale) * 100.0 <= 16383.0)
                break;
        }
    }
    const auto scaled = value * scale;
    // Native FISTP qword yields an indefinite value for NaN; its low word
    // is zero, which the unsigned conversion below observes as zero.
    const auto integer = std::isnan(scaled) ? 0.0 : std::nearbyint(static_cast<double>(scaled));
    return static_cast<float>(integer) / scale;
}
void restore_near_vector(void* player, std::ptrdiff_t offset, const Vector& desired) {
    const auto actual = read<Vector>(player, offset);
    for (unsigned i = 0; i < 3; ++i)
        if (!(std::fabs(actual[i] - desired[i]) <= 0.03125f))
            return;
    write(player, offset, desired);
}
void repair_predicted_fields(EntityCommandOwner& owner, void* player,
                             const prediction::CommandHistoryState& record) {
    const auto& b = owner.bindings;
    const auto& o = b.offsets;
    const auto tick = read<std::int32_t>(&record, 0);
    if (tick != read<std::int32_t>(player, o.tick_base) && !(record.flags & 8u) &&
        !(std::fabs(*b.prediction_time - std::bit_cast<float>(owner.state.prediction_stamp)) <=
          0.2f))
        return;
    restore_near_vector(player, o.vector_5ae32976, read<Vector>(&record, 4));
    for (unsigned i = 0; i < 3; ++i) {
        const auto desired = record.recoil_angles[i];
        if (std::fabs(read<float>(player, o.vector_1bae1168 + i * 4) - desired) <= 0.03125f)
            write(player, o.vector_1bae1168 + i * 4, desired);
    }
    restore_near_vector(player, o.vector_48442721, read<Vector>(&record, 0x1C));
    const auto height = read<float>(&record, 0x28);
    if (std::fabs(read<float>(player, o.view_offset + 8) - height) <= 0.25f)
        write(player, o.view_offset + 8, height);
    const auto scalar = read<float>(&record, 0x38);
    if (read<float>(player, o.scalar_2f33c681) == quantized_prediction_scalar(scalar))
        write(player, o.scalar_2f33c681, scalar);
    write(player, o.field_b5a66556, (record.flags >> 4) & 7u);
    write(player, o.field_fb7e3404 + 4, read<std::uint32_t>(&record, 0x50));
    write(player, o.field_fb7e3404 + 8, read<std::uint32_t>(&record, 0x54));
    for (unsigned i = 0; i < 3; ++i)
        if (read<float>(player, o.vector_0317eede + i * 4) == -0.0009765625f)
            write(player, o.vector_0317eede + i * 4, std::uint32_t(0));
}
bool convar_enabled(const void* variable) {
    for (;;) {
        const auto* parent = read<const void*>(variable, 0x1C);
        if (parent == variable)
            break;
        variable = parent;
    }
    return read<std::uint32_t>(variable, 0x30) !=
           static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(variable));
}
float convar_float(const void* variable) {
    for (;;) {
        const auto* parent = read<const void*>(variable, 0x1C);
        if (parent == variable)
            break;
        variable = parent;
    }
    return std::bit_cast<float>(
        read<std::uint32_t>(variable, 0x2C) ^
        static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(variable)));
}
void restore_revolver(EntityCommandOwner& owner, void* player, std::int32_t number) {
    const auto& b = owner.bindings;
    const auto& o = b.offsets;
    const auto handle = read<std::uint32_t>(player, o.active_weapon);
    if (handle != UINT32_MAX)
        if (auto* weapon = engine::invoke<void*>(b.entity_list, 4, handle))
            if (read<std::uint16_t>(weapon, o.weapon_definition) == 64) {
                const auto& record = owner.state.revolver[number % 150];
                if (record.command_number == number)
                    write(weapon, o.postpone_fire, record.postpone_fire);
            }
}
Vector bounds(void* entity, std::size_t slot) {
    return *engine::invoke<const Vector*>(engine::invoke<void*>(entity, 3), slot);
}
Vector add(Vector first, const Vector& second) noexcept {
    for (unsigned i = 0; i < 3; ++i)
        first[i] += second[i];
    return first;
}
}

void initialize_entity_revolver_storage(RevolverCommandPrediction* storage) noexcept {
    std::memset(storage, 0, 0x708);
}
void initialize_entity_prediction_storage(prediction::CommandHistoryEntry* storage) noexcept {
    std::memset(storage, 0, 0x5460);
    for (unsigned index = 0; index < 150; ++index) {
        storage[index].state.weapon_identity = UINT32_MAX;
        write(&storage[index].state, 0x78, UINT32_MAX);
    }
}
void initialize_entity_command_storage(EntityCommandStorage& storage) noexcept {
    initialize_entity_revolver_storage(storage.revolver.data());
    initialize_entity_prediction_storage(storage.history.data());
}
void bind_entity_command_storage(EntityCommandState& state,
                                 EntityCommandStorage& storage) noexcept {
    state.revolver = storage.revolver.data();
    state.history = storage.history.data();
    state.revolver_delay = 0.23125f;
}
void process_entity_command_phase1(EntityCommandOwner& owner, void* player, Command& command) {
    const auto& b = owner.bindings;
    const auto& o = b.offsets;
    auto& state = owner.state;
    if (!*b.startup_guard_4347bdac && !*b.startup_guard_4347bd78 && !*b.connection_checked &&
        engine::invoke<bool>(b.engine_client, 27))
        request_initial_connection_refresh(*b.connection_checked, *b.deferred_connection,
                                           b.connection_notification.get());
    const auto update_frame = *b.update_frame;
    if (update_frame <= 0)
        return;
    const auto previous = previous_number(command.number);
    const auto acknowledged = engine::invoke<std::int32_t>(b.engine_client, 205);
    const auto difference = std::uint32_t(previous) - std::uint32_t(acknowledged);
    if (difference >= 150u)
        return;
    const auto* commands = read<const Command*>(b.input, 0xF0);
    const auto& prior = commands[std::uint32_t(previous) % 150u];
    if (prior.number != previous)
        return;
    const auto slot = previous % 150;
    const bool can_restore = prior.predicted &&
                             read<std::int32_t>(player, o.prediction_context + 0x68) == previous &&
                             read<std::int32_t>(b.prediction, 0xC) == update_frame &&
                             state.history[slot].command_number == previous;
    if (can_restore) {
        if (b.restore_prediction_frame && difference > 0)
            b.restore_prediction_frame(b.prediction, 0, static_cast<std::int32_t>(difference) - 1);
    } else {
        const auto saved_player_angles = read<Vector>(player, o.prediction_angles);
        Vector saved_engine_angles;
        engine::invoke<void>(b.engine_client, 18, saved_engine_angles.data());
        engine::invoke<void>(b.engine_client, 19, b.sent_angles->data());
        write(player, o.prediction_angles, *b.sent_angles);
        engine::invoke<void>(b.prediction, 3, update_frame, true, acknowledged, previous);
        engine::invoke<void>(b.engine_client, 19, saved_engine_angles.data());
        write(player, o.prediction_angles, saved_player_angles);
    }
    if (state.history[slot].command_number == previous)
        repair_predicted_fields(owner, player, state.history[slot].state);
    restore_revolver(owner, player, previous);
    const auto tick = read<std::int32_t>(player, o.tick_base);
    write(player, o.tick_base, b.observers->adjust_tick(command.number, tick));
}

void process_entity_command_phase6(EntityCommandOwner& owner, const CommandFrame& frame,
                                   void* player, void* weapon, const Command& command,
                                   std::uint8_t send_packet) {
    const auto& b = owner.bindings;
    const auto& o = b.offsets;
    if (*b.shot_recording_enabled && frame.weapon_address &&
        network::classified_packet_weapon(read<std::uint16_t>(weapon, o.weapon_definition),
                                          b.weapon_classification) &&
        ((frame.flags & 0x100000u) || ((frame.flags & 0x400u) && (command.movement.buttons & 1u)) ||
         ((frame.flags & 2u) && (command.movement.buttons & 0x800u) &&
          (frame.weapon_id_d8 == 64 || frame.weapon_kind_124 == 0))) &&
        !send_packet)
        owner.state.last_choked_shot = command.number;
    const bool include_teammates = convar_enabled(b.teammates_are_enemies);
    const auto minimum = add(bounds(player, 1), frame.origin_10c);
    const auto maximum = add(bounds(player, 2), frame.origin_10c);
    for (std::int32_t index = 1; index <= 64; ++index) {
        auto* other = engine::invoke<void*>(b.entity_list, 3, index);
        if (!other || !engine::invoke<bool>(other, 158) ||
            read<std::uint8_t>(other, o.rejected_9d920e35) ||
            engine::invoke<std::int32_t>(other, 122) <= 0 ||
            engine::invoke<bool>(static_cast<std::byte*>(other) + 8, 9) ||
            read<std::uint8_t>(other, o.rejected_cde290d2) || other == player ||
            (!include_teammates &&
             read<std::int32_t>(player, o.team) == read<std::int32_t>(other, o.team)))
            continue;
        const auto origin = read<Vector>(other, o.network_origin);
        const auto other_minimum = add(bounds(other, 1), origin);
        const auto other_maximum = add(bounds(other, 2), origin);
        if (minimum[0] < other_maximum[0] && minimum[1] < other_maximum[1] &&
            minimum[2] < other_maximum[2] && other_minimum[0] < maximum[0] &&
            other_minimum[1] < maximum[1] && other_minimum[2] < maximum[2]) {
            invalidate_command_prediction(b.prediction);
            return;
        }
    }
}

void process_entity_prediction_entities(EntityCommandOwner& owner,
                                        const EntityPredictionEntities& records) {
    const auto& b = owner.bindings;
    const auto& o = b.offsets;
    if (!*b.physics_class)
        return;
    for (std::int32_t i = 0; i < records.count; ++i) {
        const auto index = records.entries[i].entity_index;
        if (index < 64 || (std::uint32_t(index) & 0xFFFF0000u))
            continue;
        auto* entity = engine::invoke<void*>(b.entity_list, 3, index);
        if (!entity)
            continue;
        auto* networkable = engine::invoke<void*>(entity, 4);
        if (!networkable)
            continue;
        auto* identity = engine::invoke<void*>(networkable, 2);
        if (!identity || identity != *b.physics_class)
            continue;
        if (engine::invoke<std::int32_t>(engine::invoke<void*>(entity, 3), 11) != 1 ||
            read<std::uint8_t>(entity, o.move_type) != 7)
            continue;
        if (!(engine::invoke<std::uint32_t>(engine::invoke<void*>(entity, 3), 12) & 0x800u) ||
            read<std::int32_t>(entity, o.owner_handle) != -1 ||
            (read<std::uint32_t>(entity, o.ground_flags) & 0x2000000u) ||
            (read<float>(entity, o.activity_8dbc9874) == 0.0f &&
             read<float>(entity, o.activity_68140cd8) == 0.0f))
            continue;
        auto* model = engine::invoke<void*>(static_cast<std::byte*>(entity) + 4, 8);
        if (!model)
            continue;
        const auto* name = engine::invoke<const char*>(b.model_info, 3, model);
        if (!name)
            continue;
        const auto hash = events::event_hash(name);
        const auto* categories = owner.state.model_categories;
        if (!categories)
            continue;
        const auto count = categories->buckets.size() - 1;
        auto* const end = categories->buckets[count];
        auto* node = categories->buckets[hash % count];
        while (node && node->model_hash != hash)
            node = node->next;
        if (!node)
            node = end;
        if (node != end && node->category == 0xA717EF32u)
            write(entity, o.collision_d5fd2e2c + 0x18, std::uint8_t(2));
    }
}
void observe_entity_command_recharge(EntityCommandOwner& owner, std::int32_t) noexcept {
    owner.state.prediction_stamp = read<std::uint32_t>(owner.bindings.prediction_time);
}
void repair_entity_prediction_fields(EntityCommandOwner& owner, void* player,
                                     const prediction::CommandHistoryState& record) {
    repair_predicted_fields(owner, player, record);
}
float quantize_entity_prediction_scalar(float value) noexcept {
    return quantized_prediction_scalar(value);
}
void begin_entity_prediction(EntityCommandOwner& owner, void* player, const Command& command) {
    const auto& b = owner.bindings;
    const auto& o = b.offsets;
    if (!(read<std::uint32_t>(player, o.player_flags_38ca5228) & 8u) &&
        read<std::uint8_t>(player, o.state_043fa575) ==
            read<std::uint8_t>(player, o.state_334b460c)) {
        const auto interpolated = read<Vector>(player, o.interpolated_origin_6610746e);
        const auto network = read<Vector>(player, o.network_origin);
        const auto x = interpolated[0] - network[0], y = interpolated[1] - network[1],
                   z = interpolated[2] - network[2];
        if (((y * y + x * x) + z * z) < 4096.0f)
            write(player, o.interpolated_origin_6610746e, network);
    }
    const auto previous = previous_number(command.number);
    restore_revolver(owner, player, previous);
    if (convar_float(b.interpolation_enabled) != 0.0f &&
        read<std::int32_t>(b.prediction, 0x1C) == 0 &&
        read<std::int32_t>(b.prediction, 0x20) == 0) {
        const auto& entry = owner.state.history[previous % 150];
        if (entry.command_number == previous)
            repair_predicted_fields(owner, player, entry.state);
    }
}
} //namespace skeet::client
