#pragma once
#include "tick_shift_phase_state.hpp"
#include "tick_shift_runtime.hpp"
#include "tick_shift_observers.hpp"
#include "skeet/rage/settings.hpp"
#include <vector>

namespace skeet::network {
struct TickShiftPhase1Bindings {
    const rage::GlobalSettings& rage;
    void* const& entity_list; //4346A890, read at each actual handle lookup
    std::ptrdiff_t active_weapon_cd812654, origin_2a671b42;
    void* const& record_owner_43475aac; //canonical grenade owner, shared warning records
    const std::int32_t& tick_43475a60; // first pointed word, not word+4
    const std::int32_t& target_handle_4346db1c;
    const std::int32_t& target_valid_4346db2c;
    const std::array<float, 3>& target_origin_4346db20;
    const TickShiftRechargeObserverRegistry& recharge_observers;
};

bool tick_shift_phase1_record_pending(const TickShiftPhase1Bindings&, float interval); //433B888D
void notify_tick_shift_recharge_observers(const TickShiftPhase1Bindings&, void* player,
                                       std::int32_t command_number);
// Complete433ABC86. The phase's weapon argument is unused in the original;
// its attack gate resolves the player's current weapon only when required.
void process_tick_shift_phase1(TickShiftRuntimeState&, TickShiftPhaseState&, void* player,
                            client::Command&, std::uint8_t& send_packet,
                            const TickShiftRuntimeSettings&, const TickShiftRuntimeBindings&,
                            const TickShiftPhase1Bindings&, std::uint32_t& command_flags);
} // namespace skeet::network
