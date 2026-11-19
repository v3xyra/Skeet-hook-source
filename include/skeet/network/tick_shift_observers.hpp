#pragma once
#include "tick_shift_recharge_receiver.hpp"
#include "skeet/engine/owner_registry.hpp"
#include "skeet/engine/preserved_initialization.hpp"
#include <cstdint>
#include <span>

namespace skeet {
struct ShotFeedbackState;
namespace misc {
struct ViewmodelLagState;
struct ViewmodelLagBindings;
}
namespace client {
struct PredictionHistoryOwner;
struct EntityCommandOwner;
}
}

namespace skeet::network {
// Actual43475784 registration identities. Two distinct constructors register
// quick_switch_or_grenade; the native buffer retains their actual order.
// This family is separate from prediction::ObserverRegistry at43475AB0.
enum class TickShiftRechargeObserverId : std::uint32_t {
    prediction_history = 0x434723B8U,
    quick_switch_or_grenade = 0x434727FCU,
    shot_feedback = 0x4346F5A4U,
    entity_commands = 0x43470268U,
    viewmodel_lag = 0x4346F528U,
};
struct TickShiftViewmodelRecharge {
    misc::ViewmodelLagState& state;
    const misc::ViewmodelLagBindings& bindings;
    NativeTickShiftRechargeReceiver receiver{};
};
struct TickShiftShotFeedbackObserver {
    ShotFeedbackState& state;
    const std::int32_t& last_outgoing; //live *43475B2C
    NativeTickShiftRechargeReceiver receiver{};
};

// Borrows43475784/88/8C identities from the same native data owner.
// This wrapper never initializes those cells, owns no parallel vector and
// does not release the native allocation from its C++ destructor.
class TickShiftRechargeObserverRegistry {
public:
    TickShiftRechargeObserverRegistry(engine::PreservedInitialization&, engine::JobMemory);
    TickShiftRechargeObserverRegistry(std::shared_ptr<engine::NativeDataImage>, engine::JobMemory);
    TickShiftRechargeObserverRegistry(const TickShiftRechargeObserverRegistry&) = delete;
    TickShiftRechargeObserverRegistry& operator=(const TickShiftRechargeObserverRegistry&) = delete;
    TickShiftRechargeObserverRegistry(TickShiftRechargeObserverRegistry&&) = delete;
    TickShiftRechargeObserverRegistry& operator=(TickShiftRechargeObserverRegistry&&) = delete;
    // Exact4334E132 append; source table publication remains separate at
    // each constructor's recorded write after its remaining registrations.
    void append(client::PredictionHistoryOwner&);
    void append(client::EntityCommandOwner&);
    void append(TickShiftShotFeedbackObserver&);
    void append(TickShiftViewmodelRecharge&);
    void append_noop(NativeTickShiftRechargeReceiver&, void* canonical_source);
    void publish(client::PredictionHistoryOwner&) const noexcept; //4347D06D
    void publish(client::EntityCommandOwner&) const noexcept; //4347F983
    void publish(TickShiftShotFeedbackObserver&) const noexcept; //4347F777
    void publish(TickShiftViewmodelRecharge&) const noexcept; //43486C8A
    void publish_noop(NativeTickShiftRechargeReceiver&) const noexcept; //4347DC45/434867B6
    void notify_recharge(void* player, std::int32_t command_number) const;
    void notify_generated(void* player, std::int32_t final_command, std::int32_t count) const;
    void notify_cloned(void* player, client::Command&) const;
    engine::OwnerRegistry& native_storage() const noexcept {
        return owners_;
    }
    const engine::NativeDataImage& image() const noexcept {
        return *image_;
    }
    // Sole terminal release is actual433E6194. It frees current begin through
    //433837D0, preserving begin/end/limit and all borrowed source receivers.
    void finalize_storage() const;

private:
    std::shared_ptr<engine::NativeDataImage> image_;
    engine::OwnerRegistry& owners_;
    engine::JobMemory memory_;
    void append_receiver(NativeTickShiftRechargeReceiver&, void* source);
};

// Original slot1:433B6972 /433BFDDB /433D0ABD and two real433C95B7 no-ops.
void notify_tick_shift_recharge_observers(const TickShiftRechargeObserverRegistry&, void* player,
                                       std::int32_t command_number);
// Original slot0:433C86CE /43325312 and three real4334A269 no-ops.
void notify_tick_shift_generated_observers(const TickShiftRechargeObserverRegistry&, void* player,
                                        std::int32_t final_command, std::int32_t count);
// All original slot2 entries are the same genuine RET8 at4334A269.
} //namespace skeet::network
