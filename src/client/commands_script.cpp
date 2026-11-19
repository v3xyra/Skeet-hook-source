#include "skeet/client/commands.hpp"

namespace skeet::client {
namespace {
int marshal_command(script::Bridge& bridge, void* context) {
    const auto& bindings = *static_cast<const ScriptCommandBindings*>(context);
    const auto& api = *bridge.api;
    void* const state = bridge.state;
    api.rawgeti(state, -10000, bridge.runtime_ref);
    api.rawgeti(state, -1, 1);
    api.remove(state, -2);
    bindings.push_light_userdata(state, bindings.snapshot);
    *bindings.marshalling = 1;
    const int result = api.pcall(state, 1, 1, 0);
    *bindings.marshalling = 0;
    if (result == 0)
        return 1;
    if (const char* error = api.tolstring(state, -1, nullptr))
        if (bridge.report_error)
            bridge.report_error(bridge.error_context, error);
    api.settop(state, -2);
    return -1;
}
}

int dispatch_command_script(const ScriptCommandBindings& bindings, Command& command,
                            float& move_yaw, std::uint8_t& send_packet,
                            std::uint32_t& scheduler_flags) {
    auto& bridge = *bindings.bridge;
    if (!bridge.state || bridge.runtime_ref == -1)
        return 0;
    const bool unrestricted = command_angles_unrestricted(bindings.environment);
    const auto baseline = capture_script_command(*bindings.snapshot, command, move_yaw, send_packet,
                                                 *bindings.choked_commands, scheduler_flags);
    const int completed = script::dispatch_callbacks(bridge, bindings.event_name, marshal_command,
                                                     const_cast<ScriptCommandBindings*>(&bindings));
    if (completed != 0)
        apply_script_command(*bindings.snapshot, command, move_yaw, send_packet, scheduler_flags,
                             baseline, unrestricted, command_roll_blocked(bindings.environment));
    return completed;
}

} // namespace skeet::client
