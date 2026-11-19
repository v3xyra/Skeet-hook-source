#pragma once
#include "command_phase_receiver.hpp"
#include "skeet/engine/owner_registry.hpp"
#include "skeet/engine/preserved_initialization.hpp"
#include <array>
#include <cstdint>
#include <optional>
#include <span>

namespace skeet::client {
//Exact secondary vtables written after the28 proven43345EBC registrations.
//This enumeration supplies identities, never an assumed construction order.
enum class CommandOwnerId : std::uint32_t {
    owner_4346ff8c = 0x4346FF8Cu,
    automatic_weapons = 0x4346F244u,
    quick_switch = 0x4346FC24u,
    tick_shift = 0x4346EDD4u,
    owner_43472148 = 0x43472148u,
    owner_4346eed8 = 0x4346EED8u,
    owner_4346dc2c = 0x4346DC2Cu,
    owner_43470200 = 0x43470200u,
    owner_4346f01c = 0x4346F01Cu,
    owner_43470834 = 0x43470834u,
    blockbot = 0x43471E50u,
    knife = 0x43471EB8u,
    rage = 0x43472A00u,
    owner_43470478 = 0x43470478u,
    rebuy = 0x43471F0Cu,
    penetration_reticle = 0x4346F798u,
    legit = 0x4346F5B0u,
    zeus = 0x434701A0u,
    owner_4346fe30 = 0x4346FE30u,
    owner_4346f0ac = 0x4346F0ACu,
    owner_4346f048 = 0x4346F048u,
    screen_overlay = 0x4346DCC0u,
    owner_43472998 = 0x43472998u,
    owner_434700e4 = 0x434700E4u,
    movement = 0x43470770u,
    fake_lag = 0x43470390u,
    owner_4346f1f4 = 0x4346F1F4u,
    anti_aim = 0x4346F2E0u,
};
struct CommandPhaseTable {
    CommandOwnerId id;
    std::array<std::uint32_t, 10> entries;
};
std::span<const CommandPhaseTable> native_command_phase_tables() noexcept;
std::optional<CommandOwnerId> identify_command_phase_owner(std::uint32_t vtable) noexcept;
const std::array<std::uint32_t, 10>& command_phase_entries(CommandOwnerId);
bool command_phase_is_noop(std::uint32_t entry) noexcept;

//43345EBC appends physical source secondaryreceivers into the existing
//434757A0/A4/A8 triplet. Actual constructor calls establish the order; no
//semantic registration vector or duplicate pointer buffer is constructed.
class CommandPhaseRegistry {
public:
    CommandPhaseRegistry(engine::PreservedInitialization&, engine::JobMemory, CommandOwnerDispatch);
    CommandPhaseRegistry(std::shared_ptr<engine::NativeDataImage>, engine::JobMemory,
                         CommandOwnerDispatch);
    CommandPhaseRegistry(const CommandPhaseRegistry&) = delete;
    CommandPhaseRegistry& operator=(const CommandPhaseRegistry&) = delete;
    CommandPhaseRegistry(CommandPhaseRegistry&&) = delete;
    CommandPhaseRegistry& operator=(CommandPhaseRegistry&&) = delete;
    void append(NativeCommandPhaseReceiver&, void* canonical_source);
    // Publication is separate from registration and remains at the actual
    //constructor's final secondarytable write recorded in the28row manifest.
    void publish(NativeCommandPhaseReceiver&, CommandOwnerId) const;
    void require_sources() const;
    void phase0(void* player, Command&) const;
    void phase1(void* player, void* weapon, Command&, std::uint8_t& send_packet) const;
    void phase2(void* player, void* weapon, Command&, std::array<float, 3>& incoming_angles,
                std::uint8_t& send_packet) const;
    void frame_phase(std::uint32_t phase, CommandFrame&, void* player, Command&) const;
    std::int32_t query_limit(std::int32_t running_limit) const;
    std::span<void* const> owners() const noexcept {
        return owners_.entries();
    }
    engine::OwnerRegistry& native_storage() const noexcept {
        return owners_;
    }
    const engine::NativeDataImage& image() const noexcept {
        return *image_;
    }
    const CommandOwnerDispatch& services() const noexcept {
        return services_;
    }
    //433E4A38 alone frees the current begin through433837D0. It preserves
    //all three BSS cells; C++ destruction frees neither buffer nor receivers.
    void finalize_storage() const;

private:
    std::shared_ptr<engine::NativeDataImage> image_;
    engine::OwnerRegistry& owners_;
    engine::JobMemory memory_;
    CommandOwnerDispatch services_;
};
} //namespace skeet::client
