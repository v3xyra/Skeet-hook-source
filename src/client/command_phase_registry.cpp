#include "skeet/client/command_phase_registry.hpp"
#include "skeet/client/command_owner_dispatch.hpp"
#include "skeet/engine/unload.hpp"
#include <stdexcept>

namespace skeet::client {
namespace {
constexpr std::array<CommandPhaseTable, 28> tables{{
    {CommandOwnerId::owner_4346ff8c,
     {{0x433C95B7u, 0x4337D653u, 0x433B383Eu, 0x4334A269u, 0x4334A269u, 0x4334A269u, 0x4339FB7Du,
       0x43406B4Bu, 0x4334A269u, 0x433D2AACu}}},
    {CommandOwnerId::automatic_weapons,
     {{0x433C95B7u, 0x4337D653u, 0x43337B12u, 0x4334A269u, 0x433EA2AAu, 0x4334A269u, 0x4334A269u,
       0x4334A269u, 0x4334A269u, 0x433D2AACu}}},
    {CommandOwnerId::quick_switch,
     {{0x433C95B7u, 0x4337D653u, 0x4341704Au, 0x4334A269u, 0x4334A269u, 0x4334A269u, 0x4334A269u,
       0x4334A269u, 0x4334A269u, 0x433D2AACu}}},
    {CommandOwnerId::tick_shift,
     {{0x433C95B7u, 0x433ABC86u, 0x433D6A11u, 0x433B56EDu, 0x433CE4BCu, 0x4336663Fu, 0x4334A269u,
       0x43359E37u, 0x4334A269u, 0x4340165Fu}}},
    {CommandOwnerId::owner_43472148,
     {{0x433C95B7u, 0x433654D5u, 0x43337B12u, 0x4334A269u, 0x433229CEu, 0x4334A269u, 0x4334A269u,
       0x433B22CBu, 0x4334A269u, 0x433D2AACu}}},
    {CommandOwnerId::owner_4346eed8,
     {{0x433C95B7u, 0x4337D653u, 0x43337B12u, 0x4334A269u, 0x4334A269u, 0x4334A269u, 0x4334A269u,
       0x4334A269u, 0x4334A269u, 0x433D2AACu}}},
    {CommandOwnerId::owner_4346dc2c,
     {{0x433C95B7u, 0x4337D653u, 0x43337B12u, 0x4334A269u, 0x4340C927u, 0x4334A269u, 0x4331E7ACu,
       0x4334A269u, 0x4334A269u, 0x433D2AACu}}},
    {CommandOwnerId::owner_43470200,
     {{0x433C95B7u, 0x4334BB84u, 0x43337B12u, 0x4334A269u, 0x4334A269u, 0x4334A269u, 0x433E2400u,
       0x4334A269u, 0x4334A269u, 0x433D2AACu}}},
    {CommandOwnerId::owner_4346f01c,
     {{0x433C95B7u, 0x4337D653u, 0x433E4CB6u, 0x4334A269u, 0x4334A269u, 0x4334A269u, 0x43323284u,
       0x4334A269u, 0x4334A269u, 0x433D2AACu}}},
    {CommandOwnerId::owner_43470834,
     {{0x433C95B7u, 0x4337D653u, 0x433E41ADu, 0x4334A269u, 0x4334A2C8u, 0x4334A269u, 0x4339F05Du,
       0x4334A269u, 0x4334A269u, 0x433D2AACu}}},
    {CommandOwnerId::blockbot,
     {{0x433C95B7u, 0x4337D653u, 0x433B92DCu, 0x4334A269u, 0x4334A269u, 0x4334A269u, 0x4334A269u,
       0x4334A269u, 0x4334A269u, 0x433D2AACu}}},
    {CommandOwnerId::knife,
     {{0x433C95B7u, 0x4337D653u, 0x43337B12u, 0x4334A269u, 0x433257DAu, 0x4334A269u, 0x4334A269u,
       0x4334A269u, 0x4334A269u, 0x433D2AACu}}},
    {CommandOwnerId::rage,
     {{0x433C95B7u, 0x4337D653u, 0x43358716u, 0x4334A269u, 0x4334F55Fu, 0x4334A269u, 0x4335CBFBu,
       0x4334A269u, 0x433B76DEu, 0x433D2AACu}}},
    {CommandOwnerId::owner_43470478,
     {{0x433C95B7u, 0x4337D653u, 0x43337B12u, 0x4334A269u, 0x4340029Eu, 0x4334A269u, 0x4334A269u,
       0x43386E3Eu, 0x4334A269u, 0x433D2AACu}}},
    {CommandOwnerId::rebuy,
     {{0x433C95B7u, 0x4337D653u, 0x43407336u, 0x4334A269u, 0x4334A269u, 0x4334A269u, 0x4334A269u,
       0x4334A269u, 0x4334A269u, 0x433D2AACu}}},
    {CommandOwnerId::penetration_reticle,
     {{0x433C95B7u, 0x4337D653u, 0x43337B12u, 0x4334A269u, 0x4334B1DAu, 0x4334A269u, 0x4334A269u,
       0x4334A269u, 0x4334A269u, 0x433D2AACu}}},
    {CommandOwnerId::legit,
     {{0x4332CDEAu, 0x4337D653u, 0x43337B12u, 0x4334A269u, 0x43399A38u, 0x4334A269u, 0x4334A269u,
       0x4334A269u, 0x4334A269u, 0x433D2AACu}}},
    {CommandOwnerId::zeus,
     {{0x433C95B7u, 0x4337D653u, 0x43337B12u, 0x4334A269u, 0x433A12BDu, 0x4334A269u, 0x4334A269u,
       0x4334A269u, 0x4334A269u, 0x433D2AACu}}},
    {CommandOwnerId::owner_4346fe30,
     {{0x433C95B7u, 0x4337D653u, 0x43337B12u, 0x4334A269u, 0x4334A269u, 0x4334A269u, 0x433E3C96u,
       0x4334A269u, 0x4334A269u, 0x433D2AACu}}},
    {CommandOwnerId::owner_4346f0ac,
     {{0x433C95B7u, 0x433B7F7Eu, 0x43383139u, 0x4334A269u, 0x4338AD80u, 0x4334A269u, 0x4334A269u,
       0x4339C567u, 0x4334A269u, 0x433D2AACu}}},
    {CommandOwnerId::owner_4346f048,
     {{0x433FA91Cu, 0x433AFBCCu, 0x43337B12u, 0x4334A269u, 0x4334A269u, 0x4334A269u, 0x4334A269u,
       0x4334A269u, 0x4334A269u, 0x433D2AACu}}},
    {CommandOwnerId::screen_overlay,
     {{0x433C95B7u, 0x4337D653u, 0x43337B12u, 0x4334A269u, 0x433361BAu, 0x4334A269u, 0x4334A269u,
       0x4334A269u, 0x4334A269u, 0x433D2AACu}}},
    {CommandOwnerId::owner_43472998,
     {{0x433C95B7u, 0x4337D653u, 0x43401BDBu, 0x4334A269u, 0x4334A269u, 0x4334A269u, 0x4334A269u,
       0x4334A269u, 0x4334A269u, 0x433D2AACu}}},
    {CommandOwnerId::owner_434700e4,
     {{0x433C95B7u, 0x4337D653u, 0x43337B12u, 0x4334A269u, 0x4334A269u, 0x4334A269u, 0x4339EC1Bu,
       0x4334A269u, 0x4334A269u, 0x433D2AACu}}},
    {CommandOwnerId::movement,
     {{0x433C95B7u, 0x4333F746u, 0x4334659Bu, 0x4334A269u, 0x433D1F99u, 0x4334A269u, 0x4334A269u,
       0x4334A269u, 0x4334A269u, 0x433D2AACu}}},
    {CommandOwnerId::fake_lag,
     {{0x433C95B7u, 0x4337D653u, 0x433EA911u, 0x4334A269u, 0x433A069Bu, 0x4334A269u, 0x433A0EFEu,
       0x4334A269u, 0x4334A269u, 0x4340354Cu}}},
    {CommandOwnerId::owner_4346f1f4,
     {{0x433C95B7u, 0x4337D653u, 0x43337B12u, 0x4334A269u, 0x43344A6Cu, 0x4334A269u, 0x4334A269u,
       0x4334A269u, 0x4334A269u, 0x433D2AACu}}},
    {CommandOwnerId::anti_aim,
     {{0x433C95B7u, 0x4337D653u, 0x4339F3BFu, 0x4334A269u, 0x4332D1C6u, 0x4334A269u, 0x433470E4u,
       0x4334A269u, 0x4334A269u, 0x433D2AACu}}},
}};
}
std::span<const CommandPhaseTable> native_command_phase_tables() noexcept {
    return tables;
}
std::optional<CommandOwnerId> identify_command_phase_owner(std::uint32_t vtable) noexcept {
    for (const auto& table : tables)
        if (static_cast<std::uint32_t>(table.id) == vtable)
            return table.id;
    return std::nullopt;
}
const std::array<std::uint32_t, 10>& command_phase_entries(CommandOwnerId id) {
    for (const auto& table : tables)
        if (table.id == id)
            return table.entries;
    throw std::invalid_argument("Unknown native CreateMove phase owner");
}
bool command_phase_is_noop(std::uint32_t entry) noexcept {
    return entry == 0x433C95B7u || entry == 0x4337D653u || entry == 0x43337B12u ||
           entry == 0x4334A269u;
}

namespace {
NativeCommandPhaseReceiver& source_receiver(void* object) {
    auto* const receiver = static_cast<NativeCommandPhaseReceiver*>(object);
    if (!receiver || !receiver->source || !receiver->services ||
        !is_source_command_phase_table(receiver->vtable))
        throw std::logic_error("Native command entry has no concrete source receiver binding");
    return *receiver;
}
}
CommandPhaseRegistry::CommandPhaseRegistry(engine::PreservedInitialization& initialization,
                                           engine::JobMemory memory, CommandOwnerDispatch services)
    : CommandPhaseRegistry(initialization.image_owner(), memory, services) {}
CommandPhaseRegistry::CommandPhaseRegistry(std::shared_ptr<engine::NativeDataImage> image,
                                           engine::JobMemory memory, CommandOwnerDispatch services)
    : image_(image),
      owners_(engine::require_native_data_image(image_).native_cell<engine::OwnerRegistry>(
          0x434757A0u)),
      memory_(memory), services_(services) {
    if (!memory_.allocate || !memory_.free || !memory_.process_heap)
        throw std::invalid_argument("Native command storage requires the canonical process heap");
}
void CommandPhaseRegistry::append(NativeCommandPhaseReceiver& receiver, void* source) {
    receiver.source = source;
    receiver.services = &services_;
    //43345EBC uses the same -4 sentinel, append and growth as433676A0.
    //The receiver's final table remains unpublished until its later write.
    engine::register_owner(owners_, &receiver, memory_);
}
void CommandPhaseRegistry::publish(NativeCommandPhaseReceiver& receiver, CommandOwnerId id) const {
    receiver.vtable = &source_command_phase_table(id);
}
void CommandPhaseRegistry::require_sources() const {
    for (auto* object : owners_.entries())
        (void)source_receiver(object);
}
void CommandPhaseRegistry::phase0(void* player, Command& command) const {
    auto** const end = owners_.end; //433D2F95 end precedes433D2F9B begin
    auto** cursor = owners_.begin;
    for (; cursor != end; ++cursor) {
        auto& receiver = source_receiver(*cursor);
        receiver.vtable->phase0(&receiver, player, &command);
    }
}
void CommandPhaseRegistry::phase1(void* player, void* weapon, Command& command,
                                  std::uint8_t& send) const {
    auto** const end = owners_.end; //433D32EB/32F1
    auto** cursor = owners_.begin;
    for (; cursor != end; ++cursor) {
        auto& receiver = source_receiver(*cursor);
        receiver.vtable->phase1(&receiver, player, weapon, &command, &send);
    }
}
void CommandPhaseRegistry::phase2(void* player, void* weapon, Command& command,
                                  std::array<float, 3>& angles, std::uint8_t& send) const {
    auto** const end = owners_.end; //433D3470/3476
    auto** cursor = owners_.begin;
    for (; cursor != end; ++cursor) {
        auto& receiver = source_receiver(*cursor);
        receiver.vtable->phase2(&receiver, player, weapon, &command, &angles, &send);
    }
}
void CommandPhaseRegistry::frame_phase(std::uint32_t phase, CommandFrame& frame, void* player,
                                       Command& command) const {
    if (phase < 3 || phase > 8)
        throw std::invalid_argument("Native frame phase must be3..8");
    auto** const end = owners_.end; //Each of the six loops loads end before begin.
    auto** cursor = owners_.begin;
    for (; cursor != end; ++cursor) {
        auto& receiver = source_receiver(*cursor);
        CommandFramePhaseEntry entry;
        switch (phase) {
        case 3:
            entry = receiver.vtable->phase3;
            break;
        case 4:
            entry = receiver.vtable->phase4;
            break;
        case 5:
            entry = receiver.vtable->phase5;
            break;
        case 6:
            entry = receiver.vtable->phase6;
            break;
        case 7:
            entry = receiver.vtable->phase7;
            break;
        case 8:
            entry = receiver.vtable->phase8;
            break;
        default:
            throw std::logic_error("Invalid native frame phase");
        }
        entry(&receiver, &frame, player, &command);
    }
}
std::int32_t CommandPhaseRegistry::query_limit(std::int32_t limit) const {
    auto** cursor = owners_.begin; //433D3527 begin precedes433D3530 end
    auto** const end = owners_.end;
    for (; cursor != end; ++cursor) {
        auto& receiver = source_receiver(*cursor);
        limit = receiver.vtable->query_limit(&receiver, limit);
    }
    return limit;
}
void CommandPhaseRegistry::finalize_storage() const {
    engine::UnloadSystemFunctions memory;
    memory.free_heap = memory_.free;
    memory.process_heap = memory_.process_heap;
    engine::free_native_allocation(owners_.begin, memory); //433E4A38; cells survive.
}
} //namespace skeet::client
