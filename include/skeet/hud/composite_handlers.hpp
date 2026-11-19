#pragma once
#include "composite_runtime.hpp"
#include "crosshair.hpp"
#include "feature_indicators.hpp"
#include "movement_indicators.hpp"
#include "penetration_reticle.hpp"
#include "screen_overlay.hpp"
#include "skeet/effects/view.hpp"
#include "skeet/effects/world.hpp"
#include "skeet/events/hit_marker.hpp"
#include "skeet/esp/hitbox_highlight_engine.hpp"
#include "skeet/movement/blockbot.hpp"
#include <utility>

namespace skeet::hud {
// Each binding below names a concrete recovered implementation. Referenced
// owners/configuration keep their canonical lifetime; no feature state is
// copied into the callback endpoint. Construct these at the corresponding
//433D20B5 call, in the actual owner-construction order.
template <class Binding> class ConcreteHudHandler : public HudFrameCallback {
public:
    Binding binding;
    explicit ConcreteHudHandler(Binding value) : binding(std::move(value)) {}
    ConcreteHudHandler(engine::OwnerRegistryBinding registry, Binding value)
        : ConcreteHudHandler(std::move(value)) {
        register_hud_frame(registry, this);
        publish();
    }
    void publish() noexcept {
        vtable = table_.data();
    }
    ConcreteHudHandler(const ConcreteHudHandler&) = delete;
    ConcreteHudHandler& operator=(const ConcreteHudHandler&) = delete;

private:
#if defined(_M_IX86) || defined(__i386__)
    static void __fastcall entry(void* object, ui::RenderQueue* queue, void* player,
                                 std::int32_t* cursor) {
#else
    static void entry(void* object, ui::RenderQueue* queue, void* player, std::int32_t* cursor) {
#endif
        static_cast<ConcreteHudHandler*>(object)->binding.draw(*queue, player, *cursor);
    }
    inline static const std::array<HudFrameEntry, 1> table_{entry};
};

struct FreestandingHudBinding { //43472654 ->433E8FD3
    script::Bridge& scripts;
    const anti_aim::GlobalConfig& global;
    const anti_aim::Config& config;
    anti_aim::State& state;
    const IntegerConfig& indicators;
    const esp::RuntimePlayer& local;
    const InputBitmap& input;
    const esp::IndicatorLayout& layout;
    void draw(ui::RenderQueue&, void*, std::int32_t&) const;
};
struct PingHudBinding { //434727D0 ->433DF3FC
    script::Bridge& scripts;
    const IntegerConfig& indicators;
    const esp::RuntimePlayer& local;
    const misc::PingSpikeRuntime& ping;
    const esp::IndicatorLayout& layout;
    void draw(ui::RenderQueue&, void*, std::int32_t&) const;
};
struct RageHudBinding { //43472C54 ->4337F589
    script::Bridge& scripts;
    const rage::GlobalSettings& global;
    const rage::AimbotSettings& config;
    const IntegerConfig& indicators;
    const esp::RuntimePlayer& local;
    const InputBitmap& input;
    const esp::IndicatorLayout& layout;
    const esp::AimbotVisualizationConfig& visualization;
    esp::HitboxHighlightState& highlight;
    const esp::HitboxHighlightContext& highlight_context;
    void draw(ui::RenderQueue&, void*, std::int32_t&) const;
};
struct LiveRageHudBinding { //43472C54 ->4337F589, actual incoming local entity
    script::Bridge& scripts;
    const rage::GlobalSettings& global;
    const rage::AimbotSettings& config;
    const IntegerConfig& indicators;
    const InputBitmap& input;
    const esp::IndicatorLayout& layout;
    const esp::AimbotVisualizationConfig& visualization;
    esp::HitboxHighlightState& highlight;
    const esp::HitboxHighlightEngineContext& highlight_context;
    void draw(ui::RenderQueue&, void*, std::int32_t&) const;
};
struct TickShiftHudBinding { //43472688 ->433E1B2F
    script::Bridge& scripts;
    network::TickShiftState& state;
    float& shared_timer;
    const IntegerConfig& indicators;
    const TickShiftIndicatorBindings& interfaces;
    const float& frame_time;
    const esp::IndicatorLayout& layout;
    void draw(ui::RenderQueue&, void*, std::int32_t&) const;
};
struct MovementHudBinding { //43470424 ->43384B72
    script::Bridge& scripts;
    const IntegerConfig& indicators;
    const std::uint8_t& duck_enabled;
    const rage::GlobalSettings& rage;
    const std::uint32_t& command_flags;
    const QuickPeekConfig& quick_peek;
    MovementIndicatorState state;
    const esp::RuntimePlayer& local;
    const InputBitmap& input;
    const esp::IndicatorLayout& layout;
    const esp::Projection& projection;
    void draw(ui::RenderQueue&, void*, std::int32_t&) const;
};
struct CrosshairHudBinding { //4346F878 ->433E4593
    const esp::ScreenOverlayConfig& config;
    const CrosshairBindings& interfaces;
    const std::uint8_t& suspended;
    const Point& screen_size;
    void draw(ui::RenderQueue&, void*, std::int32_t&) const;
};
struct PenetrationHudBinding { //4346DC88 ->4341CC34
    const esp::ScreenOverlayConfig& config;
    const PenetrationReticleState& state;
    const PenetrationReticleBindings& interfaces;
    const Point& screen_size;
    void draw(ui::RenderQueue&, void*, std::int32_t&) const;
};
struct ScreenOverlayHudBinding { //43472720 ->43358A6A
    const esp::ScreenOverlayConfig& config;
    const ScreenOverlayState& state;
    const std::uint32_t& life_state_offset;
    const Point& screen_size;
    const esp::Projection& projection;
    const RageVector& view_origin;
    const RageTrigonometry& trigonometry;
    void draw(ui::RenderQueue&, void*, std::int32_t&) const;
};
struct ScopeHudBinding { //43470428 ->433E273D
    effects::ViewRuntime& state;
    const Point& screen_size;
    void draw(ui::RenderQueue&, void*, std::int32_t&) const;
};
struct WorldHudBinding { //434704B8 ->4336A4E3 ->433B9D00
    effects::WorldRuntime& state;
    void draw(ui::RenderQueue&, void*, std::int32_t&) const;
};
struct HitMarkerHudBinding { //4346F6A8 ->433D066C
    events::HitMarker& state;
    const events::HitMarkerView& view;
    void draw(ui::RenderQueue&, void*, std::int32_t&) const;
};
struct PerformanceHudBinding { //4346F010 ->43325F7B
    script::Bridge& scripts;
    const rage::PerformanceSettings& config;
    const std::uint8_t& low_fps; // same owner full+40, written by its update
    const std::uint8_t* const& rage_enabled; // full+44, published by lifecycle slot1
    const rage::GlobalSettings& rage;
    const InputBitmap& input;
    const esp::IndicatorLayout& layout;
    const std::uint32_t& life_state_offset;
    void draw(ui::RenderQueue&, void*, std::int32_t&) const;
};
struct BlockbotHudBinding { //43472498 ->4331FC7C
    const BlockbotState& state;
    const esp::Projection& projection;
    const std::uint32_t& life_state_offset;
    void draw(ui::RenderQueue&, void*, std::int32_t&) const;
};
struct EmptyHudBinding { //434701CC ->4334A269 is exactly RET8
    void draw(ui::RenderQueue&, void*, std::int32_t&) const noexcept {}
};
using FreestandingHudHandler = ConcreteHudHandler<FreestandingHudBinding>;
using PingHudHandler = ConcreteHudHandler<PingHudBinding>;
using RageHudHandler = ConcreteHudHandler<RageHudBinding>;
using LiveRageHudHandler = ConcreteHudHandler<LiveRageHudBinding>;
using TickShiftHudHandler = ConcreteHudHandler<TickShiftHudBinding>;
using MovementHudHandler = ConcreteHudHandler<MovementHudBinding>;
using CrosshairHudHandler = ConcreteHudHandler<CrosshairHudBinding>;
using PenetrationHudHandler = ConcreteHudHandler<PenetrationHudBinding>;
using ScreenOverlayHudHandler = ConcreteHudHandler<ScreenOverlayHudBinding>;
using ScopeHudHandler = ConcreteHudHandler<ScopeHudBinding>;
using WorldHudHandler = ConcreteHudHandler<WorldHudBinding>;
using HitMarkerHudHandler = ConcreteHudHandler<HitMarkerHudBinding>;
using PerformanceHudHandler = ConcreteHudHandler<PerformanceHudBinding>;
using BlockbotHudHandler = ConcreteHudHandler<BlockbotHudBinding>;
using EmptyHudHandler = ConcreteHudHandler<EmptyHudBinding>;
} // namespace skeet::hud
