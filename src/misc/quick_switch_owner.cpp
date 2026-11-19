#include "skeet/misc/quick_switch_owner.hpp"
#include "skeet/misc/command_owner.hpp"
#include <array>
#include <cstring>
#include <limits>
#include <new>
#include <stdexcept>

namespace skeet::misc {
namespace {
#if defined(_M_IX86) || defined(__i386__)
#define QUICK_MEMBER __thiscall
#else
#define QUICK_MEMBER
#endif
using OwnerEntry = void(QUICK_MEMBER*)(void*);
void QUICK_MEMBER destroy_quick_switch(void*);
void QUICK_MEMBER reset_quick_switch(void*);
void QUICK_MEMBER forward_quick_switch_reset(void*);
void QUICK_MEMBER unchanged_quick_switch(void*) {} //433CCE8E RET
const std::array<OwnerEntry, 9> lifecycle_table{
    destroy_quick_switch,   unchanged_quick_switch,     unchanged_quick_switch,
    unchanged_quick_switch, unchanged_quick_switch,     reset_quick_switch,
    unchanged_quick_switch, forward_quick_switch_reset, unchanged_quick_switch};
}
class QuickSwitchLifecycleOwner {
public:
    const OwnerEntry* vtable = nullptr;
    engine::JobMemory memory;
    QuickSwitchRuntime& runtime;
    CommandOwner command;
    client::NativeCommandPhaseReceiver command_receiver{};
    network::NativeTickShiftRechargeReceiver recharge_receiver{};

    QuickSwitchLifecycleOwner(engine::OwnerRegistryBinding lifecycle,
                              client::CommandPhaseRegistry& commands,
                              network::TickShiftRechargeObserverRegistry& recharge,
                              prediction::ObserverRegistry& prediction,
                              ConfigRegistries& registries, QuickSwitchConfig& config,
                              QuickSwitchRuntime& source)
        : memory(lifecycle.memory), runtime(source), command(&source) {
        lifecycle.append(this); //433676A0
        commands.append(command_receiver, &command); //4347DC1C ->43345EBC
        recharge.append_noop(recharge_receiver,
                             &runtime); //4347DC24; final table remains unpublished
        prediction.append(runtime); //4341CEE4, table4346FA2C
        vtable = lifecycle_table.data(); //4347DC34, table4346DC9C
        commands.publish(command_receiver, client::CommandOwnerId::quick_switch); //4347DC3D
        recharge.publish_noop(recharge_receiver); //4347DC45, table434727FC
        prediction.publish(runtime); //4347DC4C, table4346FA2C
        initialize_quick_switch_config(registries, config);
        runtime.enabled = &config.enabled.value;
        reset_quick_switch(this);
    }
};
namespace {
void QUICK_MEMBER reset_quick_switch(void* object) { //4337E8DD
    auto& state = static_cast<QuickSwitchLifecycleOwner*>(object)->runtime.state;
    state.last_switch_command = -1;
    state.pending_command = std::numeric_limits<std::int32_t>::max();
}
void QUICK_MEMBER forward_quick_switch_reset(void* object) { //433CF2F5
    const auto* owner = static_cast<QuickSwitchLifecycleOwner*>(object);
    owner->vtable[5](object);
}
void QUICK_MEMBER destroy_quick_switch(void* object) { //433B1A7A ->433837D0
    auto* owner = static_cast<QuickSwitchLifecycleOwner*>(object);
    const auto memory = owner->memory;
    owner->~QuickSwitchLifecycleOwner();
    auto* bytes = static_cast<std::byte*>(object);
    std::uint32_t displacement;
    std::memcpy(&displacement, bytes - 8, sizeof displacement);
    memory.free(memory.process_heap, 0, bytes - 16 - displacement);
}
}
std::size_t quick_switch_owner_size() noexcept {
    return sizeof(QuickSwitchLifecycleOwner);
}
QuickSwitchLifecycleOwner& construct_quick_switch(
    void* storage, engine::OwnerRegistryBinding lifecycle, client::CommandPhaseRegistry& commands,
    network::TickShiftRechargeObserverRegistry& recharge, prediction::ObserverRegistry& prediction,
    ConfigRegistries& registries, QuickSwitchConfig& config, QuickSwitchRuntime& runtime) {
    const auto& image = prediction.image();
    if (&recharge.image() != &image ||
        &lifecycle.registry != &image.native_cell<engine::OwnerRegistry>(0x43475B78u))
        throw std::invalid_argument("Quick-switch observers require the retained lifecycle image");
    return *::new (storage) QuickSwitchLifecycleOwner(lifecycle, commands, recharge, prediction,
                                                      registries, config, runtime);
}
#undef QUICK_MEMBER
} //namespace skeet::misc
