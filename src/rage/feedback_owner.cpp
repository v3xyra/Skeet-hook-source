#include "skeet/rage/feedback_owner.hpp"
#include "skeet/client/input_commands.hpp"
#include "skeet/client/native_startup.hpp"
#include "skeet/engine/interface_call.hpp"
#include "skeet/legit/accuracy.hpp"
#include "skeet/misc/native_text.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstring>
#include <new>
#include <stdexcept>
#include <xmmintrin.h>

namespace skeet::rage {
namespace {
FeedbackPositionRuntime* position_runtime;
template <class T> T field(const void* object, std::ptrdiff_t offset) noexcept {
    T value;
    std::memcpy(&value, static_cast<const std::byte*>(object) + offset, sizeof value);
    return value;
}
#if defined(_M_IX86) || defined(__i386__)
#define FEEDBACK_MEMBER __thiscall
#define FEEDBACK_FAST __fastcall
#define FEEDBACK_VECTOR __vectorcall
#else
#define FEEDBACK_MEMBER
#define FEEDBACK_FAST
#define FEEDBACK_VECTOR
#endif
using PrimaryEntry = void(FEEDBACK_MEMBER*)(void*);
using PacketEntry = void(FEEDBACK_FAST*)(void*, const void*);
using PositionEntry = void(FEEDBACK_VECTOR*)(void*, float, float, float);
void FEEDBACK_MEMBER destroy_entry(void*);
void FEEDBACK_MEMBER forward_reset_entry(void*);
void FEEDBACK_MEMBER reset_entry(void*);
void FEEDBACK_MEMBER unchanged_entry(void*) {} //433CCE8E
void FEEDBACK_FAST packet_entry(void*, const void*);
void FEEDBACK_FAST frame_entry(void*, std::uint8_t);
void FEEDBACK_VECTOR position_entry(void*, float, float, float);
const std::array<PrimaryEntry, 9> primary_table{
    destroy_entry,       unchanged_entry, unchanged_entry, unchanged_entry, unchanged_entry,
    forward_reset_entry, unchanged_entry, reset_entry,     unchanged_entry};
const PacketEntry packet_table[]{packet_entry};
const engine::FrameUpdateEntry frame_table[]{frame_entry};
const PositionEntry position_table[]{position_entry};
struct PacketEndpoint {
    const PacketEntry* vtable;
    FeedbackOwner* owner;
};
struct FrameEndpoint : engine::FrameUpdateEndpoint {
    FeedbackOwner* owner;
};
struct PositionEndpoint {
    const PositionEntry* vtable;
    FeedbackOwner* owner;
};

void* allocate(engine::JobMemory memory, std::uint32_t size) { //43387448
    const auto bytes = size + 0x20u;
    auto* raw = memory.allocate(memory.process_heap, 8, bytes);
    if (!raw)
        return nullptr;
    const auto address = reinterpret_cast<std::uintptr_t>(raw);
    const auto header = (address + 15) & ~std::uintptr_t(15);
    const auto displacement = std::uint32_t(header - address);
    std::memcpy(reinterpret_cast<void*>(header + 8), &displacement, 4);
    std::memcpy(reinterpret_cast<void*>(header + 12), &bytes, 4);
    return reinterpret_cast<void*>(header + 16);
}
void free(engine::JobMemory memory, void* object) { //433837D0
    auto* bytes = static_cast<std::byte*>(object);
    std::uint32_t displacement;
    std::memcpy(&displacement, bytes - 8, 4);
    memory.free(memory.process_heap, 0, bytes - 16 - displacement);
}
bool shotgun_patterns(const void* variable) {
    for (;;) {
        const auto* parent = field<const void*>(variable, 0x1C);
        if (parent == variable)
            break;
        variable = parent;
    }
    return field<std::uint32_t>(variable, 0x30) !=
           std::uint32_t(reinterpret_cast<std::uintptr_t>(variable));
}
std::uint32_t packet_round_integer(float value) noexcept {
    std::int64_t integer;
#if (defined(__i386__) || defined(__x86_64__)) && (defined(__GNUC__) || defined(__clang__))
    __asm__ __volatile__("flds %1\n\tfistpq %0" : "=m"(integer) : "m"(value) : "st");
#elif defined(_M_IX86) && defined(_MSC_VER)
    __asm {
        fld dword ptr value
        fistp qword ptr integer
    }
#else
#error Feedback packet quantization requires the native x87 FISTP boundary
#endif
    return std::uint32_t(std::bit_cast<std::uint64_t>(integer));
}
//433FD134 at this packet body's four fixed0..maximum call sites. SSE controls
// scale conversion/retry; the final x87 FISTP uses the current rounding mode.
float packet_quantize(float value, unsigned bits, float maximum) noexcept {
    const auto upper = (std::uint32_t(1) << bits) - 1u;
    const double extent = static_cast<double>(maximum);
    const auto initial =
        _mm_cvtss_f32(_mm_cvtpd_ps(_mm_div_sd(_mm_set_sd(double(upper)), _mm_set_sd(extent))));
    auto scale = initial;
    if (static_cast<double>(scale) * extent > double(upper)) {
        constexpr float factors[]{0.9998999834060669f, 0.9900000095367432f, 0.8999999761581421f,
                                  0.800000011920929f, 0.699999988079071f};
        bool selected = false;
        for (const auto factor : factors) {
            scale = _mm_cvtss_f32(_mm_mul_ss(_mm_set_ss(factor), _mm_set_ss(initial)));
            if (static_cast<double>(scale) * extent <= double(upper)) {
                selected = true;
                break;
            }
        }
        if (!selected)
            scale = 0.0f;
    }
    if (value < 0.0f)
        return 0.0f;
    if (value > maximum)
        return maximum;
    const auto scaled = _mm_cvtss_f32(_mm_mul_ss(_mm_set_ss(value), _mm_set_ss(scale)));
    // Indefinite qword's low dword is zero for NaN. The native unsigned
    // low-dword conversion also turns the rounded signed zero into+0.
    const auto integer = packet_round_integer(scaled);
    const auto converted = _mm_cvtss_f32(_mm_cvtpd_ps(_mm_set_sd(double(integer))));
    return _mm_cvtss_f32(_mm_div_ss(_mm_set_ss(converted), _mm_set_ss(scale)));
}
float packet_angle(float value) noexcept {
    const auto scaled = _mm_mul_ss(_mm_set_ss(value), _mm_set_ss(182.04444885253906f));
    const auto wrapped = std::uint32_t(_mm_cvttss_si32(scaled)) & 0xFFFFu;
    const auto degrees = _mm_cvtss_f32(_mm_mul_ss(
        _mm_cvtsi32_ss(_mm_setzero_ps(), std::int32_t(wrapped)), _mm_set_ss(0.0054931640625f)));
    return packet_quantize(degrees, 13, 359.9560546875f);
}
void fire_listener(void*, events::GameEventReader&, events::EventPlayerIndices&);
void hurt_listener(void*, events::GameEventReader&, events::EventPlayerIndices&);
void death_listener(void*, events::GameEventReader&, events::EventPlayerIndices&);
void impact_listener(void*, events::GameEventReader&, events::EventPlayerIndices&);
void require_canonical_bindings(const FeedbackOwnerBindings& bindings) {
    if (&bindings.runtime.spread_log_settings != &bindings.settings ||
        &bindings.packet.recording_enabled != &bindings.live.recording_enabled)
        throw std::invalid_argument(
            "Feedback owner requires its canonical settings and recording gate");
}
}

class FeedbackOwner {
public:
    const PrimaryEntry* vtable = nullptr;
    const FeedbackOwnerBindings& bindings;
    FeedbackCommandOwner commands;
    client::NativeCommandPhaseReceiver command_receiver;
    FeedbackEntityOwner entities;
    client::NativeEntityObserverReceiver entity_receiver;
    network::TickShiftShotFeedbackObserver recharge;
    PacketEndpoint packet;
    FrameEndpoint frame;
    PositionEndpoint position;
    void* engine_client; //full native+34, captured at the recorded MOV
    misc::NativeEncryptedText spread_message;
    events::EventListener impact;
    std::uint8_t impact_subscribed;

    explicit FeedbackOwner(const FeedbackOwnerBindings& b)
        : bindings(b), commands{b.state, b.last_outgoing_command, b.live.recording_enabled},
          entities{b.state, b.runtime, b.live}, recharge{b.state, b.last_outgoing_command} {
        b.lifecycle.append(this); //4347F719, primary pointer is still null
        b.commands.append(command_receiver, &commands); //4347F721, native+8
        b.entities.append_feedback(entity_receiver, entities); //4347F729, native+10
        packet.owner = this;
        b.packets.append(&packet); //+18,4347F731
        frame.owner = this;
        engine::register_frame_update(b.frames, &frame); //+20
        position.owner = this;
        b.positions.append(&position); //+28,4347F758
        b.recharge.append(recharge); //4347F760, source table still unpublished
        packet.vtable = packet_table;
        frame.vtable = frame_table; //4347F767/770
        b.recharge.publish(recharge); //4347F777, before primary4347F77E
        vtable = primary_table.data();
        b.commands.publish(command_receiver, client::CommandOwnerId::owner_4346dc2c); //4347F784
        b.entities.publish_feedback(entity_receiver); //4347F78E
        position.vtable = position_table;
        //mem_int3.h:497 records4347F79C MOVEAX,[4346A514].
        engine_client = b.engine_client;
        initialize_shot_log_settings(b.configuration, b.settings);
        b.state.pending.clear();
        b.state.events.has_pending_shots = false;
        constexpr char message[] = "Missed shot due to spread";
        static_assert(sizeof(message) == 26);
        misc::construct_native_encrypted_text(spread_message, message, 25, b.lifecycle.memory);
        b.state.recent_position_count = 0;
        b.state.events.feedback.clear();
        b.state.fire_snapshots.clear();
        impact = {events::bullet_impact_hash, this, impact_listener};
        impact_subscribed = 0;
        b.events.add({events::weapon_fire_hash, this, fire_listener});
        b.events.add({events::player_hurt_hash, this, hurt_listener});
        b.events.add({events::player_death_hash, this, death_listener});
    }
    void toggle_impact(std::uint8_t desired) { //complete4336B697
        if (desired == impact_subscribed)
            return;
        impact_subscribed = desired;
        if (desired)
            bindings.events.add(impact);
        else
            bindings.events.remove(impact);
    }
    void prepare_event() noexcept {
        auto& state = bindings.state;
        state.events.enabled = bindings.live.recording_enabled != 0;
        state.events.has_pending_shots = !state.pending.empty();
    }
    ~FeedbackOwner() { //complete433CDB60; permanent listener lifetime is external
        toggle_impact(0);
        std::deque<ShotFireSnapshot>{}.swap(bindings.state.fire_snapshots);
        std::vector<events::ShotEvent>{}.swap(bindings.state.events.feedback);
        misc::destroy_native_encrypted_text(spread_message, bindings.lifecycle.memory);
        std::vector<ShotFeedbackRecord>{}.swap(bindings.state.pending);
        bindings.state.events.has_pending_shots = false;
    }
};
static_assert(sizeof(FeedbackOwner) <= 0x110);

namespace {
void FEEDBACK_MEMBER destroy_entry(void* object) {
    auto* owner = static_cast<FeedbackOwner*>(object);
    destroy_feedback_owner(owner, owner->bindings.active);
}
void FEEDBACK_MEMBER forward_reset_entry(void* object) { //433B06E3 forwards to7
    auto* owner = static_cast<FeedbackOwner*>(object);
    owner->vtable[7](object);
}
void FEEDBACK_MEMBER reset_entry(void* object) {
    reset_feedback_owner(*static_cast<FeedbackOwner*>(object));
}
void FEEDBACK_FAST packet_entry(void* object, const void* packet) {
    capture_feedback_packet(*static_cast<PacketEndpoint*>(object)->owner, packet);
}
void FEEDBACK_FAST frame_entry(void* object, std::uint8_t update) {
    update_feedback_subscription(*static_cast<FrameEndpoint*>(object)->owner, update);
}
void FEEDBACK_VECTOR position_entry(void* object, float x, float y, float z) {
    capture_feedback_position(*static_cast<PositionEndpoint*>(object)->owner, {x, y, z});
}
void fire_listener(void* object, events::GameEventReader& event,
                   events::EventPlayerIndices& players) {
    auto& owner = *static_cast<FeedbackOwner*>(object);
    owner.prepare_event();
    events::weapon_fire(owner.bindings.state.events, event, players);
}
void hurt_listener(void* object, events::GameEventReader& event,
                   events::EventPlayerIndices& players) {
    auto& owner = *static_cast<FeedbackOwner*>(object);
    owner.prepare_event();
    events::player_hurt(owner.bindings.state.events, event, players);
}
void death_listener(void* object, events::GameEventReader& event,
                    events::EventPlayerIndices& players) {
    auto& owner = *static_cast<FeedbackOwner*>(object);
    owner.prepare_event();
    events::player_death(owner.bindings.state.events, event, players);
}
void impact_listener(void* object, events::GameEventReader& event,
                     events::EventPlayerIndices& players) {
    auto& owner = *static_cast<FeedbackOwner*>(object);
    owner.prepare_event();
    events::bullet_impact(owner.bindings.state.events, event, players);
}
void initialize_feedback_owner(void* context) {
    create_feedback_owner(*static_cast<FeedbackOwnerBindings*>(context));
}
}

FeedbackPacketOffsets feedback_packet_offsets(const engine::NativeOffsetStorage& storage) {
    const auto& g = storage.globals;
    return {{g, 0x43467EC0, 0x434678BC, 0xB6F85C08, 0}, {g, 0x43468910, 0x43468418, 0x2A671B42, 0},
            {g, 0x43468E90, 0x43468F20, 0xE9392423, 0}, {g, 0x43467480, 0x4346850C, 0xC6C62EBA, 0},
            {g, 0x4346A45C, 0x4346A324, 0x6777C9CE, 0}, {g, 0x43468FAC, 0x43467BEC, 0xF62F1430, 0},
            {g, 0x434693FC, 0x43468B6C, 0xF300F6B0, 0}, {g, 0x43467034, 0x434670D8, 0xEC1739AA, 0},
            {g, 0x43468B28, 0x4346A61C, 0x571B7D9A, 0}};
}
void reset_feedback_owner(FeedbackOwner& owner) noexcept {
    auto& state = owner.bindings.state;
    state.pending.clear();
    state.events.feedback.clear();
    state.fire_snapshots.clear();
    state.events.has_pending_shots = false;
}
void update_feedback_subscription(FeedbackOwner& owner, std::uint8_t update) {
    if (update)
        owner.toggle_impact(owner.bindings.live.recording_enabled);
}
void capture_feedback_position(FeedbackOwner& owner, const RageVector& position) noexcept {
    const RageVector captured = position; //XMM0/1/2 are saved before the native shift
    auto& state = owner.bindings.state;
    const auto incremented = state.recent_position_count + 1u;
    const auto count = std::min(incremented, 10u);
    state.recent_position_count = count;
    if (count != 1u)
        std::memmove(state.recent_positions.data() + 2, state.recent_positions.data() + 1,
                     std::size_t(count - 1u) * sizeof(RageVector));
    std::memcpy(&state.recent_positions[1], &captured, sizeof captured);
}
void capture_feedback_packet(FeedbackOwner& owner, const void* packet) {
    const auto& b = owner.bindings.packet;
    auto& state = owner.bindings.state;
    if (!b.capture_enabled || !b.recording_enabled)
        return;
    const auto player = field<std::uint32_t>(packet, b.offsets.player_index);
    if (player > 63u)
        return;
    if (player + 1u != std::uint32_t(engine::invoke<std::int32_t>(owner.engine_client, 12)))
        return;
    if (state.pending.empty())
        return;
    state.fire_snapshots.emplace_back();
    auto& snapshot = state.fire_snapshots.back();
    snapshot.marker =
        std::uint32_t(engine::invoke<std::int32_t>(owner.bindings.engine_client, 205));
    snapshot.origin = field<RageVector>(packet, b.offsets.origin);
    ShotFeedbackRecord* candidate = nullptr;
    std::uint32_t count = 0;
    for (auto& record : state.pending) {
        const auto current =
            std::uint32_t(engine::invoke<std::int32_t>(owner.bindings.engine_client, 205));
        if (record.words[3] == current) {
            ++count;
            candidate = &record;
        }
    }
    auto spread = field<float>(packet, b.offsets.spread);
    auto inaccuracy = field<float>(packet, b.offsets.inaccuracy);
    auto recoil = field<float>(packet, b.offsets.recoil_index);
    auto angles = field<RageVector>(packet, b.offsets.angles);
    angles[2] = 0.0f;
    if (candidate && count == 1u) {
        const auto first = std::bit_cast<std::int32_t>(candidate->words[1]);
        if (const auto* command = client::find_input_command(b.input, first)) {
            const auto pitch =
                packet_angle(command->angles[0] + std::bit_cast<float>(candidate->words[24]));
            const auto yaw =
                packet_angle(command->angles[1] + std::bit_cast<float>(candidate->words[25]));
            if (std::fabs(pitch - angles[0]) < 0.001f && std::fabs(yaw - angles[1]) < 0.001f) {
                angles[0] = pitch;
                angles[1] = yaw;
            }
        }
        const auto raw_inaccuracy = std::bit_cast<float>(candidate->words[22]);
        if (std::fabs(packet_quantize(raw_inaccuracy, 10, 1.0f) - inaccuracy) < 0.001f)
            inaccuracy = raw_inaccuracy;
        const auto raw_recoil = std::bit_cast<float>(candidate->words[21]);
        if (std::fabs(packet_quantize(raw_recoil, 10, 1000.0f) - recoil) < 0.001f)
            recoil = raw_recoil;
        const auto raw_spread = std::bit_cast<float>(candidate->words[23]);
        if (std::fabs(packet_quantize(raw_spread, 8, 0.10000000149011612f) - spread) < 0.001f)
            spread = raw_spread;
    }
    const auto seed = field<std::uint32_t>(packet, b.offsets.seed);
    const auto mode = field<std::int32_t>(packet, b.offsets.fire_mode);
    const auto weapon = field<std::uint16_t>(packet, b.offsets.weapon_id);
    const SpreadParameters parameters{weapon,
                                      mode,
                                      spread,
                                      minimum_number(1.0f, inaccuracy),
                                      recoil,
                                      1,
                                      shotgun_patterns(b.shotgun_patterns)};
    const auto sample = generate_spread_sample(parameters, seed, b.spread_tables, b.random,
                                               b.shotgun_pattern, b.trigonometry);
    const auto basis = legit::spread_basis(angles, b.trigonometry);
    RageVector direction;
    for (std::size_t i = 0; i < 3; ++i)
        direction[i] =
            (basis.first[i] * sample.first + basis.center[i]) + basis.second[i] * sample.second;
    const auto square =
        (direction[0] * direction[0] + direction[1] * direction[1]) + direction[2] * direction[2];
    const auto inverse = 1.0f / (std::sqrt(square) + 0x1p-23f);
    for (std::size_t i = 0; i < 3; ++i)
        snapshot.direction[i] = direction[i] * inverse;
}
void dispatch_feedback_position_packet(const engine::OwnerRegistry& registry, const void* packet) {
    if ((field<std::uint32_t>(packet, 0x1C) & 0xBu) != 0xBu)
        return;
    const auto x = field<float>(packet, 8), y = field<float>(packet, 0xC);
    auto* cursor = registry.begin;
    const auto z = field<float>(packet, 0x14);
    auto* const end = registry.end;
    for (; cursor != end; ++cursor) {
        auto* endpoint = *cursor;
        (*static_cast<const PositionEntry* const*>(endpoint))[0](endpoint, x, y, z);
    }
}
void bind_feedback_position_runtime(FeedbackPositionRuntime& runtime) noexcept {
    position_runtime = &runtime;
}
void unbind_feedback_position_runtime(FeedbackPositionRuntime& runtime) noexcept {
    if (position_runtime == &runtime)
        position_runtime = nullptr;
}
#if defined(_M_IX86) || defined(__i386__)
void __cdecl feedback_position_hook(const void* packet) {
#else
void feedback_position_hook(const void* packet) {
#endif
    dispatch_feedback_position_packet(position_runtime->positions, packet);
}
std::size_t feedback_owner_size() noexcept {
    return sizeof(FeedbackOwner);
}
FeedbackOwner& construct_feedback_owner(void* storage, const FeedbackOwnerBindings& bindings) {
    require_canonical_bindings(bindings);
    return *::new (storage) FeedbackOwner(bindings);
}
FeedbackOwner* create_feedback_owner(const FeedbackOwnerBindings& bindings) {
    require_canonical_bindings(bindings);
    auto* storage = allocate(bindings.lifecycle.memory, 0x110);
    if (!storage) {
        bindings.active = nullptr;
        return nullptr;
    }
    // These borrowed cells belong to the native HeapZeroMemory allocation.
    // The ordinary constructor below still preserves unwritten position slots.
    bindings.settings = ShotLogSettings{};
    bindings.state.recent_positions = {};
    auto& owner = construct_feedback_owner(storage, bindings);
    bindings.active = &owner; //mem_int3.h:561,43485C53 MOV[43475A7C],EAX
    return &owner;
}
void bind_feedback_owner_factory(client::NativeStartupSources& sources,
                                 FeedbackOwnerBindings& bindings) {
    sources.bind_initializer({0x43485C3A, &bindings, initialize_feedback_owner});
}
void destroy_feedback_owner(FeedbackOwner* owner, FeedbackOwner*& active) {
    active = nullptr;
    if (!owner)
        return;
    owner->bindings.active = nullptr;
    const auto memory = owner->bindings.lifecycle.memory;
    owner->~FeedbackOwner();
    free(memory, owner);
}
void* feedback_lifecycle_object(FeedbackOwner& owner) noexcept {
    return &owner;
}
ShotFeedbackState* feedback_owner_state(FeedbackOwner* owner) noexcept {
    return owner ? &owner->bindings.state : nullptr;
}
const ShotFeedbackState* feedback_owner_state(const FeedbackOwner* owner) noexcept {
    return owner ? &owner->bindings.state : nullptr;
}
const ShotLogSettings* feedback_owner_settings(const FeedbackOwner* owner) noexcept {
    return owner ? &owner->bindings.settings : nullptr;
}
const ShotFeedbackRuntime* feedback_owner_runtime(const FeedbackOwner* owner) noexcept {
    return owner ? &owner->bindings.runtime : nullptr;
}

#undef FEEDBACK_MEMBER
#undef FEEDBACK_FAST
#undef FEEDBACK_VECTOR
} //namespace skeet::rage
