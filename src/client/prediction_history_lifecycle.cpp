#include "skeet/client/prediction_history_lifecycle.hpp"
#include "skeet/client/native_startup.hpp"
#include "skeet/animation/state_copy.hpp"
#include <bit>
#include <cstring>
#include <new>
#include <stdexcept>

namespace skeet::client {
namespace {
#if defined(_M_IX86) || defined(__i386__)
#define PREDICTION_HISTORY_MEMBER __thiscall
#else
#define PREDICTION_HISTORY_MEMBER
#endif
using PrimaryEntry = void(PREDICTION_HISTORY_MEMBER*)(void*);

void* allocate(engine::JobMemory memory, std::size_t size) {
    auto* const raw = memory.allocate(memory.process_heap, 8, size + 0x20);
    if (!raw)
        return nullptr;
    const auto base = reinterpret_cast<std::uintptr_t>(raw);
    const auto header = (base + 15) & ~std::uintptr_t(15);
    const auto displacement = std::uint32_t(header - base), total = std::uint32_t(size + 0x20);
    std::memcpy(reinterpret_cast<void*>(header + 8), &displacement, 4);
    std::memcpy(reinterpret_cast<void*>(header + 12), &total, 4);
    return reinterpret_cast<void*>(header + 16);
}
void release(engine::JobMemory memory, void* object) {
    auto* const bytes = static_cast<std::byte*>(object);
    std::uint32_t displacement;
    std::memcpy(&displacement, bytes - 8, 4);
    memory.free(memory.process_heap, 0, bytes - 16 - displacement);
}
void construct_string(PredictionHistoryStorage& state, std::size_t index, const char* text,
                      engine::JobMemory memory) {
    //433B2213 ->433C1360. All22decoded native literals are nonempty.
    state.bind_sequence_string(index, {});
    const auto length = std::strlen(text);
    auto* const bytes = static_cast<char*>(allocate(memory, length + 1));
    PredictionHistoryString descriptor{bytes, bytes, bytes + length + 1};
    state.bind_sequence_string(index, descriptor); //433C1394..139C, before memmove
    // The native nonempty path does not establish an allocation-failure
    // continuation. Do not substitute a literal, empty text or another buffer.
    std::memmove(bytes, text, length);
    descriptor.end = bytes + length;
    state.bind_sequence_string(index, descriptor); //433C13B1, before terminator
    bytes[length] = 0;
}
void destroy_string(const PredictionHistoryString& text, engine::JobMemory memory) {
    const auto capacity =
        std::bit_cast<std::int32_t>(std::uint32_t(reinterpret_cast<std::uintptr_t>(text.limit)) -
                                    std::uint32_t(reinterpret_cast<std::uintptr_t>(text.begin)));
    if (capacity > 1 && text.begin)
        release(memory, const_cast<char*>(text.begin));
    //4336B1E9 preserves its descriptor words after release.
}

void PREDICTION_HISTORY_MEMBER destruction_entry(void*);
void PREDICTION_HISTORY_MEMBER settings_entry(void*);
void PREDICTION_HISTORY_MEMBER reset_entry(void*);
void PREDICTION_HISTORY_MEMBER refresh_entry(void*);
void PREDICTION_HISTORY_MEMBER unchanged_entry(void*) {} //actual433CCE8E
const std::array<PrimaryEntry, 9> primary_table{
    destruction_entry, settings_entry,  unchanged_entry, unchanged_entry, unchanged_entry,
    reset_entry,       unchanged_entry, refresh_entry,   unchanged_entry};

struct Instance {
    const PrimaryEntry* vtable = nullptr;
    NativeCommandPhaseReceiver command_receiver;
    NativeEntityObserverReceiver entity_receiver;
    PredictionHistoryLifecycleBindings bindings;
    ConfigRegistries& configuration;
    engine::JobMemory memory;

    Instance(const PredictionHistoryRegistries& registries, ConfigRegistries& config,
             PredictionHistoryLifecycleBindings context)
        : bindings(context), configuration(config), memory(registries.primary.memory) {
        auto& owner = bindings.owner;
        auto& state = bindings.storage;
        owner.canonical_storage = &state;
        registries.primary.append(this); //4347D024
        registries.commands.append(command_receiver, &owner); //02C
        registries.empty_ticks.append(owner); //034, table still unpublished
        registries.prediction.append(owner); //03C
        registries.entities.append_prediction_history(entity_receiver, owner); //044
        registries.data_updates.append(owner); //04C, table still unpublished
        registries.recharge.append(owner); //054, table still unpublished
        registries.entities.publish_prediction_history(entity_receiver); //05B
        registries.data_updates.publish(owner); //064
        registries.recharge.publish(owner); //06D
        vtable = primary_table.data(); //079
        registries.commands.publish(command_receiver, CommandOwnerId::owner_4346ff8c); //07F
        registries.empty_ticks.publish(owner); //086
        registries.prediction.publish(owner); //08D

        //4347D094's MOV is recorded in supplied mem_int3.h:462; raw is CCx5.
        state.initialize_header(bindings.acknowledged_command); //094..09C
        auto* const snapshot_allocation = static_cast<std::byte*>(allocate(memory, 0x3112C)); //09F
        network::TickShiftCommandSnapshot* snapshots = nullptr;
        if (snapshot_allocation) {
            constexpr std::uint32_t count = 150;
            std::memcpy(snapshot_allocation, &count, 4);
            snapshots = ::new (snapshot_allocation + 4) network::TickShiftCommandSnapshot[150];
            for (std::size_t index = 0; index < 150; ++index)
                animation::initialize_animation_state_vector(
                    reinterpret_cast<std::byte*>(snapshots + index) + 0x1F4);
        }
        state.bind_snapshots(snapshots, snapshots ? 150u : 0u); //0D1, including native null branch
        auto* const record_allocation = allocate(memory, 0x2C88); //0D4
        auto* const records = record_allocation ? ::new (record_allocation)
                                                      RestrictedCommandPrediction0x4C[150]
                                                : nullptr;
        state.bind_prediction_records(records, records ? 150u : 0u); //0D9
        state.redirect_from_44 = -1; //0DF
        state.redirect_to_48 = -1; //0E3
        const auto strings = prediction_history_sequence_strings();
        for (std::size_t index = 0; index < strings.size(); ++index)
            construct_string(state, index, strings[index], memory); //0DC..D868
    }
};
static_assert(sizeof(Instance) <= 0x65C);

void require_binding_identity(const PredictionHistoryRegistries& registries,
                              const PredictionHistoryLifecycleBindings& bindings) {
    if (&registries.recharge.image() != &registries.data_updates.image() ||
        &registries.prediction.image() != &registries.data_updates.image() ||
        &registries.empty_ticks.image() != &registries.data_updates.image())
        throw std::invalid_argument(
            "Prediction history observer registries must share their preserved image");
    if (&bindings.active !=
        &registries.data_updates.image().native_cell<PredictionHistoryOwner*>(0x434759B4u))
        throw std::invalid_argument(
            "Prediction history lifecycle requires the preserved434759B4 owner cell");
    auto& state = bindings.storage;
    const auto& owner = bindings.owner;
    if (&owner.shared != &state.shared() || &owner.redirect_from_44 != &state.redirect_from_44 ||
        &owner.redirect_to_48 != &state.redirect_to_48 ||
        (owner.canonical_storage && owner.canonical_storage != &state))
        throw std::invalid_argument(
            "Prediction history lifecycle requires its canonical owner and storage");
    if (state.has_standalone_backing() || state.shared().snapshots ||
        state.shared().prediction_records)
        throw std::invalid_argument(
            "Prediction history lifecycle requires deferred unbound native buffers");
    for (const auto& text : state.sequence_strings)
        if (text.begin || text.end || text.limit)
            throw std::invalid_argument(
                "Prediction history lifecycle requires unbound owned strings");
}

void PREDICTION_HISTORY_MEMBER destruction_entry(void* storage) {
    auto& instance = *static_cast<Instance*>(storage);
    destroy_prediction_history_owner(storage, instance.bindings.active);
}
void PREDICTION_HISTORY_MEMBER settings_entry(void* storage) {
    bind_prediction_history_owner_settings(storage);
}
void PREDICTION_HISTORY_MEMBER reset_entry(void* storage) {
    reset_prediction_history_owner(storage);
}
void PREDICTION_HISTORY_MEMBER refresh_entry(void* storage) {
    refresh_prediction_history_owner(storage);
}
void initialize_entry(void* context) {
    auto& source = *static_cast<PredictionHistoryInitialization*>(context);
    create_prediction_history_owner(source.registries, source.configuration, source.bindings);
}
#undef PREDICTION_HISTORY_MEMBER
}

std::size_t prediction_history_owner_size() noexcept {
    return 0x65C;
} //4349081E allocation extent
void* construct_prediction_history_owner(void* storage,
                                         const PredictionHistoryRegistries& registries,
                                         ConfigRegistries& configuration,
                                         PredictionHistoryLifecycleBindings bindings) {
    require_binding_identity(registries, bindings);
    return ::new (storage) Instance(registries, configuration, bindings);
}
void* create_prediction_history_owner(const PredictionHistoryRegistries& registries,
                                      ConfigRegistries& configuration,
                                      PredictionHistoryLifecycleBindings bindings) {
    require_binding_identity(registries, bindings);
    void* const storage = allocate(registries.primary.memory, prediction_history_owner_size());
    if (storage) {
        construct_prediction_history_owner(storage, registries, configuration, bindings);
        bindings.active = &bindings.owner; //43490837, mem_int3.h:654 MOV record
    } else
        bindings.active = nullptr; //ordinary raw XOR at0835, then same recorded store
    return storage;
}
void destroy_prediction_history_owner(void* storage, PredictionHistoryOwner*& active) {
    active = nullptr; //433B7312, before null test
    if (!storage)
        return;
    auto* const instance = static_cast<Instance*>(storage);
    auto& state = instance->bindings.storage;
    const auto memory = instance->memory;
    for (std::size_t index = state.sequence_strings.size(); index != 0; --index)
        destroy_string(state.sequence_strings[index - 1], memory);
    if (auto* records = state.shared().prediction_records) {
        release(memory, records);
        state.bind_prediction_records(nullptr, 0); //433AE465
    }
    if (auto* snapshots = state.shared().snapshots) {
        auto* const cookie = reinterpret_cast<std::byte*>(snapshots) - 4;
        std::uint32_t count;
        std::memcpy(&count, cookie, 4);
        //433FB67D visits the genuine count-cookie array in reverse.
        while (count) {
            --count;
            animation::release_animation_state_vector(
                reinterpret_cast<std::byte*>(snapshots + count) + 0x1F4,
                instance->bindings.animation_allocator);
        }
        release(memory, cookie);
        state.bind_snapshots(nullptr, 0); //433F925D
    }
    instance->~Instance();
    release(memory, storage);
}
void bind_prediction_history_owner_settings(void* storage) {
    auto& instance = *static_cast<Instance*>(storage);
    instance.bindings.storage.field_34 =
        &static_cast<IntegerConfig*>(find_config(instance.configuration.descriptors, 0x8F8A4366u))
             ->value;
}
void reset_prediction_history_owner(void* storage) {
    auto& state = static_cast<Instance*>(storage)->bindings.storage;
    for (std::size_t index = 0; index < 150; ++index)
        state.shared().snapshots[index].command_number = -1;
    for (std::size_t index = 0; index < 150; ++index)
        state.shared().prediction_records[index].command_number = -1;
    state.command_tags.fill(UINT32_MAX);
    state.landing_tags.fill(UINT32_MAX);
    state.redirect_from_44 = -1;
    state.redirect_to_48 = -1;
}
void refresh_prediction_history_owner(void* storage) {
    auto& instance = *static_cast<Instance*>(storage);
    instance.vtable[5](storage); //433CF2F5 calls the current primary slot5
}
void bind_prediction_history_initializer(NativeStartupSources& sources,
                                         PredictionHistoryInitialization& context) {
    sources.bind_initializer({0x4349081Eu, &context, initialize_entry});
}
} // namespace skeet::client
