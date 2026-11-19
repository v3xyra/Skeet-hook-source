#include "skeet/misc/viewmodel_lag_owner.hpp"
#include "skeet/client/native_startup.hpp"
#include <array>
#include <cstring>
#include <new>
#include <stdexcept>

namespace skeet::misc {
namespace {
#if defined(_M_IX86) || defined(__i386__)
#define VIEWMODEL_MEMBER __thiscall
#define VIEWMODEL_FAST __fastcall
#else
#define VIEWMODEL_MEMBER
#define VIEWMODEL_FAST
#endif
using PrimaryEntry = void(VIEWMODEL_MEMBER*)(void*);
struct FrameEndpoint : engine::FrameUpdateEndpoint {
    ViewmodelLagOwner* owner;
};
void VIEWMODEL_MEMBER destruction_entry(void*);
void VIEWMODEL_MEMBER reset_entry(void*);
void VIEWMODEL_MEMBER refresh_entry(void*);
void VIEWMODEL_MEMBER unchanged_entry(void*) {} //433CCE8E
void VIEWMODEL_FAST frame_entry(void*, std::uint8_t);
const std::array<PrimaryEntry, 9> primary_table{
    destruction_entry, unchanged_entry, unchanged_entry, unchanged_entry, unchanged_entry,
    reset_entry,       unchanged_entry, refresh_entry,   unchanged_entry};
const std::array<engine::FrameUpdateEntry, 1> frame_table{frame_entry};
void require_owner_bindings(const ViewmodelLagOwnerRegistries& registries,
                            ViewmodelHooksRuntime& hooks) {
    const auto& image = hooks.image();
    if (&registries.prediction.image() != &image || &registries.recharge.image() != &image)
        throw std::invalid_argument(
            "Viewmodel prediction and recharge registries require the hook initialization image");
    if (&registries.primary.registry != &image.native_cell<engine::OwnerRegistry>(0x43475B78u) ||
        &registries.frames.registry != &image.native_cell<engine::OwnerRegistry>(0x43475AD8u) ||
        &hooks.shared_owner() != &image.native_cell<ViewmodelLagOwner*>(viewmodel_lag_owner_slot))
        throw std::invalid_argument(
            "Viewmodel owner requires canonical lifecycle/frame registries and owner slot");
}
} //namespace

class ViewmodelLagOwner {
public:
    const PrimaryEntry* vtable = nullptr;
    ViewmodelHooksRuntime& hooks;
    engine::JobMemory memory;
    prediction::ViewmodelPredictionObserver prediction;
    client::NativeEntityObserverReceiver entity_receiver{};
    client::NativeCommandPhaseReceiver command_receiver{};
    network::TickShiftViewmodelRecharge recharge;
    FrameEndpoint frame{{nullptr}, this};

    ViewmodelLagOwner(const ViewmodelLagOwnerRegistries& registries, ViewmodelHooksRuntime& runtime)
        : hooks(runtime), memory(registries.primary.memory),
          prediction{hooks.owner_state(), hooks.feature_bindings()},
          recharge{hooks.owner_state(), hooks.feature_bindings()} {
        //43486C44..43486C73: these are six separate registrar calls. The two
        //semantic views below borrow the same owner; neither owns another ring.
        registries.primary.append(this);
        registries.prediction.append(prediction);
        registries.entities.append_viewmodel_lag(entity_receiver, prediction); //43486C57
        registries.commands.append(command_receiver, &hooks.owner_state()); //43486C5F
        registries.recharge.append(recharge); //43486C67, table still unpublished
        engine::register_frame_update(registries.frames, &frame);

        registries.entities.publish_viewmodel_lag(entity_receiver); //43486C76, table43472644
        registries.commands.publish(command_receiver,
                                    client::CommandOwnerId::owner_4346f048); //43486C7E
        registries.recharge.publish(recharge); //43486C8A, table4346F528
        //43486C91 clears+44 before publishing frame/primary/prediction tables.
        //All six registrations precede the first final endpoint table write.
        auto& state = hooks.owner_state();
        state.previous_animation_time = 0;
        frame.vtable = frame_table.data();
        vtable = primary_table.data();
        registries.prediction.publish(prediction); //43486CAA, table4346F0D4
        //43486CB2..BC writes only the weapon-handle word at+500+index*20.
        for (auto& snapshot : state.snapshots)
            snapshot.weapon_handle = 0xFFFFFFFFu;
    }
};
static_assert(sizeof(ViewmodelLagOwner) <= viewmodel_lag_owner_allocation);

namespace {
void VIEWMODEL_MEMBER destruction_entry(void* storage) {
    auto* const owner = static_cast<ViewmodelLagOwner*>(storage);
    destroy_viewmodel_lag_owner(owner, owner->hooks.shared_owner());
}
void VIEWMODEL_MEMBER reset_entry(void* storage) {
    reset_viewmodel_lag_owner(static_cast<ViewmodelLagOwner*>(storage));
}
void VIEWMODEL_MEMBER refresh_entry(void* storage) {
    refresh_viewmodel_lag_owner(static_cast<ViewmodelLagOwner*>(storage));
}
void VIEWMODEL_FAST frame_entry(void* receiver, std::uint8_t) { //4337D213, DL unused
    auto& frame = *static_cast<FrameEndpoint*>(static_cast<engine::FrameUpdateEndpoint*>(receiver));
    consume_viewmodel_prediction(frame.owner->hooks.owner_state());
}
void factory_entry(void* opaque) {
    auto& context = *static_cast<ViewmodelLagFactoryContext*>(opaque);
    create_viewmodel_lag_owner(context.registries, context.hooks);
}

void* allocate(engine::JobMemory memory, std::size_t size) { //43387448
    auto* const allocation = memory.allocate(memory.process_heap, 8, size + 0x20);
    if (!allocation)
        return nullptr;
    const auto base = reinterpret_cast<std::uintptr_t>(allocation);
    const auto header = (base + 15) & ~std::uintptr_t(15);
    const auto displacement = std::uint32_t(header - base), total = std::uint32_t(size + 0x20);
    std::memcpy(reinterpret_cast<void*>(header + 8), &displacement, 4);
    std::memcpy(reinterpret_cast<void*>(header + 12), &total, 4);
    return reinterpret_cast<void*>(header + 16);
}
#undef VIEWMODEL_MEMBER
#undef VIEWMODEL_FAST
} //namespace

std::size_t viewmodel_lag_owner_size() noexcept {
    return viewmodel_lag_owner_allocation;
}
ViewmodelLagOwner* construct_viewmodel_lag_owner(void* storage,
                                                 const ViewmodelLagOwnerRegistries& registries,
                                                 ViewmodelHooksRuntime& hooks) {
    require_owner_bindings(registries, hooks);
    return ::new (storage) ViewmodelLagOwner(registries, hooks);
}
ViewmodelLagOwner* create_viewmodel_lag_owner(const ViewmodelLagOwnerRegistries& registries,
                                              ViewmodelHooksRuntime& hooks) {
    require_owner_bindings(registries, hooks);
    auto* const storage = allocate(registries.primary.memory, viewmodel_lag_owner_size());
    ViewmodelLagOwner* owner = nullptr;
    if (storage)
        owner = construct_viewmodel_lag_owner(storage, registries, hooks);
    // Supplied mem_int3.h instruction record restores this actual store. Its
    // source value is this invocation's constructor result, including null.
    hooks.shared_owner() = owner;
    return owner;
}
ViewmodelLagState* viewmodel_lag_owner_state(ViewmodelLagOwner* owner) noexcept {
    return owner ? &owner->hooks.owner_state() : nullptr;
}
void destroy_viewmodel_lag_owner(ViewmodelLagOwner* storage, ViewmodelLagOwner*& active) {
    active = nullptr; //433A102F, before433837D0's null check
    if (!storage)
        return;
    auto* const owner = storage;
    const auto memory = owner->memory;
    owner->~ViewmodelLagOwner();
    auto* const bytes = reinterpret_cast<std::byte*>(storage);
    std::uint32_t displacement;
    std::memcpy(&displacement, bytes - 8, 4);
    memory.free(memory.process_heap, 0, bytes - 16 - displacement);
}
void reset_viewmodel_lag_owner(ViewmodelLagOwner* owner) {
    reset_viewmodel_lag(owner->hooks.owner_state());
}
void refresh_viewmodel_lag_owner(ViewmodelLagOwner* owner) {
    owner->vtable[5](owner);
}
void bind_viewmodel_lag_factory(client::NativeStartupSources& sources,
                                ViewmodelLagFactoryContext& context) {
    if (&sources.data() != &context.hooks.image())
        throw std::invalid_argument(
            "Viewmodel factory requires the scheduler's canonical data owner");
    require_owner_bindings(context.registries, context.hooks);
    sources.bind_initializer({0x4347E13Eu, &context, factory_entry});
}
} //namespace skeet::misc
