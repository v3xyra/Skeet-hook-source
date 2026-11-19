#pragma once
#include "data_update_receiver.hpp"
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
namespace skins {
class NativeAgentRuntime;
}
namespace network {
struct TickShiftAcknowledgementOwner;
enum class DataUpdateObserverId : std::uint32_t {
    prediction_history = 0x43470720U,
    agents = 0x43472978U,
    tick_shift = 0x43472100U,
    entity_commands = 0x4346F814U,
};
struct DataUpdateObserverTable {
    DataUpdateObserverId id;
    std::array<std::uint32_t, 2> methods;
};
std::span<const DataUpdateObserverTable> native_data_update_observer_tables() noexcept;
std::optional<DataUpdateObserverId> identify_data_update_observer(std::uint32_t) noexcept;
// Borrowed native BSS43475760/64/68. Its allocation contains only pointers to
// the four concrete source secondary interfaces, never variant payloads.
// Construction borrows the retained image without resetting its cells.
class DataUpdateObserverRegistry {
public:
    DataUpdateObserverRegistry(engine::PreservedInitialization&, engine::JobMemory);
    DataUpdateObserverRegistry(std::shared_ptr<engine::NativeDataImage>, engine::JobMemory);
    DataUpdateObserverRegistry(const DataUpdateObserverRegistry&) = delete;
    DataUpdateObserverRegistry& operator=(const DataUpdateObserverRegistry&) = delete;
    DataUpdateObserverRegistry(DataUpdateObserverRegistry&&) = delete;
    DataUpdateObserverRegistry& operator=(DataUpdateObserverRegistry&&) = delete;

    // Registrar4337EFD9 uses the same433F1665/4334DC99 append as main owners.
    // Actual constructor calls supply registration order; metadata tables do
    // not select or reorder source owners. Each owner must retain its address.
    void append(client::PredictionHistoryOwner&);
    void append(skins::NativeAgentRuntime&);
    void append(TickShiftAcknowledgementOwner&);
    void append(client::EntityCommandOwner&);
    // Constructors publish the actual final tables after their remaining
    // registrations, at4347D064/DACA/DF38/F9A5 respectively.
    void publish(client::PredictionHistoryOwner&) const noexcept;
    void publish(skins::NativeAgentRuntime&) const noexcept;
    void publish(TickShiftAcknowledgementOwner&) const noexcept;
    void publish(client::EntityCommandOwner&) const noexcept;
    std::span<void* const> owners() const noexcept {
        return owners_.entries();
    }
    engine::OwnerRegistry& native_storage() const noexcept {
        return owners_;
    }
    const engine::NativeDataImage& image() const noexcept {
        return *image_;
    }
    // Original fastcall entries receive the player in EDX and update on stack.
    // Both dispatchers capture the vector range once, as their native callers.
    void capture_forward(void* player, std::int32_t update) const; //433B7613 slot0
    void finish_reverse(void* player, std::int32_t update) const; //433B7093 slot1
    // Only actual433F4A1D owns terminal buffer release. C++ destruction does
    // not free it; the callback leaves all three BSS pointers unchanged.
    void finalize_storage() const;

private:
    std::shared_ptr<engine::NativeDataImage> image_;
    engine::OwnerRegistry& owners_;
    engine::JobMemory memory_;
    void append_receiver(NativeDataUpdateReceiver&, void*);
};
} //namespace network
} //namespace skeet
