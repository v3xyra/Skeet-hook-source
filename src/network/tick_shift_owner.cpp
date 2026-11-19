#include "skeet/network/tick_shift_owner.hpp"
#include "skeet/network/tick_shift_phase1.hpp"
#include "skeet/network/tick_shift_phase7.hpp"
#include <array>
#include <cstring>
#include <new>
#include <stdexcept>

namespace skeet::network {
namespace {
#if defined(_M_IX86) || defined(__i386__)
#define TICK_SHIFT_MEMBER __thiscall
#define TICK_SHIFT_FAST __fastcall
#else
#define TICK_SHIFT_MEMBER
#define TICK_SHIFT_FAST
#endif
using PrimaryEntry = void(TICK_SHIFT_MEMBER*)(void*);
struct TickShiftInstance;
struct HudEndpoint : hud::HudFrameCallback {
    TickShiftInstance* owner;
};
void TICK_SHIFT_MEMBER destroy_entry(void*);
void TICK_SHIFT_MEMBER unchanged(void*) {} //actual433CCE8E
void TICK_SHIFT_MEMBER reset_entry(void*);
void TICK_SHIFT_MEMBER refresh_entry(void*);
void TICK_SHIFT_FAST hud_entry(void*, ui::RenderQueue*, void*, std::int32_t*);
const std::array<PrimaryEntry, 9> primary_table{destroy_entry, unchanged,     unchanged,
                                                unchanged,     unchanged,     reset_entry,
                                                unchanged,     refresh_entry, unchanged};
const std::array<hud::HudFrameEntry, 1> hud_table{hud_entry};

void require_source_bindings(const TickShiftOwnerRegistries& registries,
                             const TickShiftOwnerBindings& bindings) {
    const auto* const image = &registries.data_updates.image();
    const auto& command = bindings.command;
    if (&registries.prediction.image() != image ||
        &command.phase7.prediction_observers.image() != image ||
        &command.bindings.recharge_observers.image() != image ||
        &command.phase1.recharge_observers.image() != image ||
        &command.phase7.generated_observers.image() != image)
        throw std::invalid_argument("Tick-shift phases must share the retained observer registries");
    auto& snapshots = image->native_cell<client::PredictionHistoryOwner*>(0x434759b4u);
    auto& history = image->native_cell<client::EntityCommandOwner*>(0x43475898u);
    if (&command.bindings.prediction_snapshots_owner != &snapshots ||
        &command.phase7.restricted_owner != &snapshots ||
        &command.bindings.prediction_history_owner != &history)
        throw std::invalid_argument("Tick-shift phases must read the native history owner slots");
    if (&registries.primary.registry != &image->native_cell<engine::OwnerRegistry>(0x43475b78u) ||
        &registries.hud.registry != &image->native_cell<engine::OwnerRegistry>(0x43475a9cu) ||
        &registries.messages.registry != &image->native_cell<engine::OwnerRegistry>(0x43475b94u))
        throw std::invalid_argument(
            "Tick-shift construction requires the native lifecycle, HUD and message registries");
}

struct TickShiftInstance {
    const PrimaryEntry* vtable = nullptr;
    engine::JobMemory memory;
    TickShiftOwnerBindings bindings;
    HudEndpoint hud{{nullptr}, this};
    TickShiftMessageHandler message;
    client::NativeCommandPhaseReceiver command_receiver;
    TickShiftAcknowledgementOwner acknowledgement;

    TickShiftInstance(const TickShiftOwnerRegistries& registries, ConfigRegistries& config,
                    TickShiftOwnerBindings context)
        : memory(registries.primary.memory), bindings(context),
          message(context.command.runtime.network, context.messages),
          acknowledgement{context.command.runtime, context.command.phases,
                          context.acknowledgement} {
        registries.primary.append(this); //433676A0, full+0
        hud::register_hud_frame(registries.hud, &hud); //433D20B5, full+8
        registries.messages.append(&message); //4336F73C, full+10
        registries.commands.append(command_receiver, &bindings.command); //4347DF11,+18
        registries.data_updates.append(acknowledgement); //4337EFD9,+20
        registries.prediction.append(bindings.command.runtime); //4341CEE4,+28
        message.publish_table(); //native4346F240
        registries.commands.publish(command_receiver, client::CommandOwnerId::tick_shift); //4347DF31
        registries.data_updates.publish(acknowledgement); //4347DF38, after prediction registration
        registries.prediction.publish(bindings.command.runtime); //4347DF3F, table4346F7D0
        vtable = primary_table.data(); //native434707D0
        hud.vtable = hud_table.data(); //native43472688
        rage::initialize_tick_shift_settings(config, bindings.command.controls.settings);
        auto& runtime = bindings.command.runtime;
        auto& state = runtime.network;
        state.mode = 0;
        state.last_command = -1;
        bindings.command.phases.observed_ack_10c = -1;
        acknowledgement.captured_tick_base_118 = -1;
        state.timer_124 = 0.0f;
        runtime.transition_command_110 = 0x7FFFFFFF;
        state.shot_tick = 0x7FFFFFFF;
        bindings.command.phases.recharge_ready_command_11c = 0x7FFFFFFF;
        state.budget = 16;
        state.available = 0;
        runtime.low_health_120 = 0;
        state.random.seed = 0;
        state.random.previous = 0;
        state.history_first = 0;
        state.history_count = 0;
        //4347DEEB leaves flags/limits/rings/random-table/other scalar words.
    }
};
static_assert(sizeof(TickShiftInstance) <= native_tick_shift_owner_allocation_size);
void TICK_SHIFT_MEMBER destroy_entry(void* storage) {
    destroy_tick_shift_owner(storage);
}
void TICK_SHIFT_MEMBER reset_entry(void* storage) {
    reset_tick_shift_command_owner(static_cast<TickShiftInstance*>(storage)->bindings.command);
}
void TICK_SHIFT_MEMBER refresh_entry(void* storage) {
    refresh_tick_shift_owner(storage);
}
void TICK_SHIFT_FAST hud_entry(void* endpoint, ui::RenderQueue* queue, void* player,
                            std::int32_t* cursor) {
    auto& owner = *static_cast<HudEndpoint*>(static_cast<hud::HudFrameCallback*>(endpoint))->owner;
    const auto& b = owner.bindings.hud;
    hud::draw_tick_shift_indicators(*queue, b.scripts, owner.bindings.command.runtime.network,
                                 b.shared_timer, b.indicators, player, b.interfaces, b.frame_time,
                                 *cursor, b.layout);
}
void* allocate(engine::JobMemory memory, std::size_t size) {
    auto* const raw = memory.allocate(memory.process_heap, 8, size + 0x20);
    if (!raw)
        return nullptr;
    const auto address = reinterpret_cast<std::uintptr_t>(raw);
    const auto header = (address + 15) & ~std::uintptr_t(15);
    const auto displacement = std::uint32_t(header - address);
    const auto total = std::uint32_t(size + 0x20);
    std::memcpy(reinterpret_cast<void*>(header + 8), &displacement, 4);
    std::memcpy(reinterpret_cast<void*>(header + 12), &total, 4);
    return reinterpret_cast<void*>(header + 16);
}
}
std::size_t tick_shift_owner_size() noexcept {
    return sizeof(TickShiftInstance);
}
void construct_tick_shift_owner(void* storage, const TickShiftOwnerRegistries& registries,
                             ConfigRegistries& config, TickShiftOwnerBindings bindings) {
    require_source_bindings(registries, bindings);
    ::new (storage) TickShiftInstance(registries, config, bindings);
}
void* allocate_tick_shift_owner_storage(engine::JobMemory memory) {
    return allocate(memory, native_tick_shift_owner_allocation_size);
}
void construct_allocated_tick_shift_owner(void* storage, const TickShiftOwnerRegistries& registries,
                                       ConfigRegistries& config, TickShiftOwnerBindings bindings) {
    require_source_bindings(registries, bindings);
    //43387448 uses HEAP_ZERO_MEMORY. Canonical value fields live outside the
    // semantic endpoint allocation, so project those zeroed native words here.
    // Placement construction itself deliberately preserves untouched words.
    auto& command = bindings.command;
    command.controls.settings = rage::TickShiftSettings{};
    auto& runtime = command.runtime;
    runtime.network = TickShiftState{};
    runtime.field_f0 = 0;
    runtime.transition_command_110 = 0;
    runtime.low_health_120 = 0;
    runtime.field_130 = 0;
    runtime.last_flush_command_134 = 0;
    runtime.command_ticks.fill(TickShiftCommandTick{0, 0});
    runtime.generated_commands.fill(0);
    command.phases.observed_ack_10c = 0;
    command.phases.recharge_ready_command_11c = 0;
    command.weapon_command_12c = 0;
    construct_tick_shift_owner(storage, registries, config, bindings);
}
void destroy_tick_shift_owner(void* storage) {
    if (!storage)
        return; //actual433837D0 null guard
    auto* const owner = static_cast<TickShiftInstance*>(storage);
    const auto memory = owner->memory;
    owner->~TickShiftInstance();
    auto* const bytes = static_cast<std::byte*>(storage);
    std::uint32_t displacement;
    std::memcpy(&displacement, bytes - 8, 4);
    memory.free(memory.process_heap, 0, bytes - 16 - displacement);
}
void refresh_tick_shift_owner(void* storage) {
    auto& owner = *static_cast<TickShiftInstance*>(storage);
    owner.vtable[5](storage);
}
#undef TICK_SHIFT_MEMBER
#undef TICK_SHIFT_FAST
} //namespace skeet::network
