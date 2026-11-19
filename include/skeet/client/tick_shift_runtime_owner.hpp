#pragma once
#include <memory>

namespace skeet::engine {
class SceneQueryRuntimeOwner;
}
namespace skeet::network {
struct TickShiftCommandOwner;
struct TickShiftState;
}
namespace skeet::script {
class SourceScriptRuntimeOwner;
}
namespace skeet::client {
class ClientRuntimeStorage;
class ConfigurationRuntimeOwner;
class HistoryRuntimeOwner;
class MovementHudRuntimeOwner;
class CommandRuntimeOwner;
class LocalAnimationRuntimeOwner;
class VisualRuntimeOwner;
class NativeStartupSources;
struct EngineRuntimeBindings;
struct TickShiftRuntimeFactoryBindings;
// One network/tickbase state shared by all six concrete tick-shift endpoints.
// Construction composes live inputs only; the4347FF2F factory still allocates,
// registers and initializes in the scheduler's recovered order.
class TickShiftRuntimeOwner final {
public:
    TickShiftRuntimeOwner(ClientRuntimeStorage&, const EngineRuntimeBindings&,
                        ConfigurationRuntimeOwner&, HistoryRuntimeOwner&, MovementHudRuntimeOwner&,
                        CommandRuntimeOwner&, LocalAnimationRuntimeOwner&,
                        engine::SceneQueryRuntimeOwner&, VisualRuntimeOwner&,
                        script::SourceScriptRuntimeOwner&);
    ~TickShiftRuntimeOwner();
    TickShiftRuntimeOwner(const TickShiftRuntimeOwner&) = delete;
    TickShiftRuntimeOwner& operator=(const TickShiftRuntimeOwner&) = delete;
    TickShiftRuntimeOwner(TickShiftRuntimeOwner&&) = delete;
    TickShiftRuntimeOwner& operator=(TickShiftRuntimeOwner&&) = delete;
    ClientRuntimeStorage& storage() const noexcept;
    network::TickShiftCommandOwner& command() noexcept;
    network::TickShiftState& state() noexcept;
    TickShiftRuntimeFactoryBindings& factory_bindings() noexcept;
    void bind_initializers(NativeStartupSources&);

private:
    struct Data;
    std::unique_ptr<Data> data_;
};
} //namespace skeet::client
