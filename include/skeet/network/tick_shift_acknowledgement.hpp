#pragma once
#include "tick_shift_runtime.hpp"
#include "tick_shift_phase_state.hpp"
#include "data_update_receiver.hpp"
#include "skeet/engine/startup_convars.hpp"

namespace skeet::network {
struct TickShiftAcknowledgementBindings {
    void* const& engine_client; //4346A514
    void* const& engine_globals; //43467B04, mode+18/receive interval+20
    std::int32_t* const& acknowledged; //43475BA4
    const std::int32_t* const& old_acknowledged; //43475794
    float* const& interval; //4346A338
    const std::int32_t& process_milliseconds; //43468290
    engine::ConVarBinding acknowledgement_convar; //4346A850 xor478720C2
    std::uint32_t rejected_9d920e35, tick_base_3d10b94c;
};
// Registered receiverfull+20, vtable43472100. It borrows the same command/tick
// and generated-tag rings used by the tick-shift CreateMove phases.
struct TickShiftAcknowledgementOwner {
    TickShiftRuntimeState& runtime;
    TickShiftPhaseState& phases;
    const TickShiftAcknowledgementBindings& bindings;
    std::int32_t captured_tick_base_118 = -1; //full+130, constructor4347DEEB
    NativeDataUpdateReceiver data_update_receiver{};
};
void capture_tick_shift_acknowledgement(TickShiftAcknowledgementOwner&, void* player); //433CCC20
void finish_tick_shift_acknowledgement(TickShiftAcknowledgementOwner&, void* player); //433EEA67
} // namespace skeet::network
