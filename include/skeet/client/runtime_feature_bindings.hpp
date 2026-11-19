#pragma once
#include "combat_runtime_factories.hpp"
#include <memory>

namespace skeet::anti_aim {
class RuntimeOwner;
}
namespace skeet::effects {
class EffectsRuntimeOwner;
}
namespace skeet::script {
class SourceScriptRuntimeOwner;
struct SourceScriptModuleInputs;
}
namespace skeet::client {
class ClientRuntimeStorage;
class PacketRuntimeOwner;
class TickShiftRuntimeOwner;
struct NativeStartupSourceGraph;

// The five combat factories borrow their already composed physical domain
// owners. This aggregate creates no feature owner, Lua state, callback entry
// or registry receiver. All source referents outlive factory execution.
class CombatRuntimeOwner final {
public:
    // SourceScriptRuntimeOwner's genuine module inputs must already be bound.
    CombatRuntimeOwner(ClientRuntimeStorage&, PacketRuntimeOwner&, TickShiftRuntimeOwner&,
                       anti_aim::RuntimeOwner&, effects::EffectsRuntimeOwner&,
                       script::SourceScriptRuntimeOwner&);
    // The late production phase may bind those actual module inputs here,
    // after CONFIG/RootMenu contexts exist and before ScriptOwner is created.
    CombatRuntimeOwner(ClientRuntimeStorage&, PacketRuntimeOwner&, TickShiftRuntimeOwner&,
                       anti_aim::RuntimeOwner&, effects::EffectsRuntimeOwner&,
                       script::SourceScriptRuntimeOwner&, script::SourceScriptModuleInputs);
    ~CombatRuntimeOwner();
    CombatRuntimeOwner(const CombatRuntimeOwner&) = delete;
    CombatRuntimeOwner& operator=(const CombatRuntimeOwner&) = delete;
    CombatRuntimeOwner(CombatRuntimeOwner&&) = delete;
    CombatRuntimeOwner& operator=(CombatRuntimeOwner&&) = delete;
    ClientRuntimeStorage& storage() const noexcept;
    CombatRuntimeFactoryBindings& factory_bindings() noexcept;
    void bind_initializers(NativeStartupSources&);

private:
    struct Data;
    std::unique_ptr<Data> data_;
};

// Late source composition after genuine engine acquisition. Validates the
// graph's shared native cells and all existing factory contexts, binds the57
// recovered factory identities and preflights that exact plan. It neither
// constructs features nor executes/replaces/reselects any CRT, terminal
// cleanup or final43484F06 binding. The graph and its referents stay alive.
void bind_runtime_feature_sources(NativeStartupSources&, NativeStartupSourceGraph&);
} //namespace skeet::client
