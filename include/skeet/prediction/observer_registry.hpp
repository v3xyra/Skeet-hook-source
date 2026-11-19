#pragma once
#include "skeet/client/commands.hpp"
#include "skeet/events/event_name_storage.hpp"
#include "skeet/prediction/observer_receiver.hpp"
#include "skeet/engine/owner_registry.hpp"
#include "skeet/engine/preserved_initialization.hpp"
#include <array>
#include <cstdint>
#include <optional>
#include <span>

namespace skeet {
namespace client {
struct PredictionHistoryOwner;
struct EntityCommandOwner;
}
namespace misc {
class QuickSwitchRuntime;
struct ViewmodelLagState;
struct ViewmodelLagBindings;
}
namespace network {
struct TickShiftRuntimeState;
}
namespace script {
struct Bridge;
}
namespace prediction {
struct ObserverRegistry;
struct PlayerCommandContext;
enum class ObserverOwnerId : std::uint32_t {
    prediction_history = 0x43472060u,
    quick_switch = 0x4346FA2Cu,
    tick_shift = 0x4346F7D0u,
    entity_commands = 0x43470924u,
    script = 0x43472B88u,
    viewmodel_lag = 0x4346F0D4u,
};
struct ObserverTable {
    ObserverOwnerId id;
    std::array<std::uint32_t, 4> methods;
};
std::span<const ObserverTable> native_prediction_observer_tables() noexcept;
std::optional<ObserverOwnerId> identify_prediction_observer(std::uint32_t vtable) noexcept;

// These two views borrow the same source state used by their other native
// interfaces. They do not own another script runtime or viewmodel history.
struct ScriptPredictionObserver {
    ScriptPredictionObserver(script::Bridge&, ObserverRegistry&);
    script::Bridge& bridge;
    events::EventNameRef before_event; // live434795C0 row, 433F48F6
    events::EventNameRef after_event; // live434795D0 row, 433C13C5
    events::EventNameRef command_number_field; // live43479580 row, exactly14bytes
    NativePredictionObserverReceiver receiver{};
};
struct ViewmodelPredictionObserver {
    misc::ViewmodelLagState& state;
    const misc::ViewmodelLagBindings& bindings;
    NativePredictionObserverReceiver receiver{};
};

// Registrar4341CEE4 appends source secondary-interface pointers into actual
//43475AB0/AB4/AB8. Borrowed native data cells are never reset here; no
// semantic vector/variant or second native buffer is constructed.
struct ObserverRegistry {
    ObserverRegistry(engine::PreservedInitialization&, engine::JobMemory);
    ObserverRegistry(std::shared_ptr<engine::NativeDataImage>, engine::JobMemory);
    ObserverRegistry(const ObserverRegistry&) = delete;
    ObserverRegistry& operator=(const ObserverRegistry&) = delete;
    ObserverRegistry(ObserverRegistry&&) = delete;
    ObserverRegistry& operator=(ObserverRegistry&&) = delete;
    void append(client::PredictionHistoryOwner&);
    void append(misc::QuickSwitchRuntime&);
    void append(network::TickShiftRuntimeState&);
    void append(client::EntityCommandOwner&);
    void append(ScriptPredictionObserver&);
    void append(ViewmodelPredictionObserver&);
    // Each actual constructor publishes its final table after its remaining
    // registrar calls, at the separately proven native write below.
    void publish(client::PredictionHistoryOwner&) const noexcept; //4347D08D
    void publish(misc::QuickSwitchRuntime&) const noexcept; //4347DC4C
    void publish(network::TickShiftRuntimeState&) const noexcept; //4347DF3F
    void publish(client::EntityCommandOwner&) const noexcept; //4347F99E
    void publish(ScriptPredictionObserver&) const noexcept; //4347FFC9
    void publish(ViewmodelPredictionObserver&) const noexcept; //43486CAA
    std::span<void* const> owners() const noexcept {
        return owners_.entries();
    }
    engine::OwnerRegistry& native_storage() const noexcept {
        return owners_;
    }
    const engine::NativeDataImage& image() const noexcept {
        return *image_;
    }

    // Native ECX=registered secondaryreceiver, EDX=player, stack=Command*.
    void capture_before(void* player, const client::Command&) const; // slot0forward
    void finish_prediction(void* player, const client::Command&) const; // slot1reverse
    void finish_forward(void* player, const client::Command&) const; // 4335A298
    // Slot3 receives the context BASE; ordinary slots0/1 receive its Command
    // at+4. Keeping the distinct input avoids treating command_number as tick.
    void rejected(void* player, const PlayerCommandContext&) const; // slot3forward
    // Native ECX=receiver, EDX=commandnumber, stack=incomingtick; each EAX
    // becomes the next owner's incoming tick. All six tables are handled.
    std::int32_t adjust_tick(std::int32_t command_number, std::int32_t tick) const;
    // Sole terminal release is actual433F515B: null-test current begin,
    // free its433837D0 allocation and leave all three cells unchanged.
    // C++ destruction releases no pointer buffer or borrowed source owner.
    void finalize_storage() const;

private:
    std::shared_ptr<engine::NativeDataImage> image_;
    engine::OwnerRegistry& owners_;
    engine::JobMemory memory_;
    void append_receiver(NativePredictionObserverReceiver&, void*);
};
std::int32_t adjust_observer_tickbases(const ObserverRegistry&, const client::Command&,
                                       std::int32_t tick);
} // namespace prediction
} // namespace skeet
