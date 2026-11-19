#include "skeet/esp/player_engine.hpp"
#include "skeet/engine/interface_call.hpp"
#include <bit>
#include <cstring>

namespace skeet::esp {
namespace {
template <class T> T field(const void* entity, std::ptrdiff_t offset) {
    T result;
    std::memcpy(&result, static_cast<const std::byte*>(entity) + offset, sizeof result);
    return result;
}
std::int32_t convar(const void* value) {
    while (value != field<const void*>(value, 0x1c))
        value = field<const void*>(value, 0x1c);
    return std::bit_cast<std::int32_t>(field<std::uint32_t>(value, 0x30) ^
                                       std::uint32_t(reinterpret_cast<std::uintptr_t>(value)));
}
}
EnginePlayers::EnginePlayers(PlayerEngineBindings bindings) : bindings_(bindings) {
    runtime_.context = this;
    runtime_.by_index = [](void* c, std::int32_t i) {
        return static_cast<EnginePlayers*>(c)->by_index(i);
    };
    runtime_.by_handle = [](void* c, std::int32_t h) {
        return static_cast<EnginePlayers*>(c)->by_handle(h);
    };
    runtime_.is_player = [](void* c, const RuntimePlayer& p) {
        return static_cast<EnginePlayers*>(c)->is_player(p);
    };
    runtime_.index = [](void* c, const RuntimePlayer& p) {
        return static_cast<EnginePlayers*>(c)->index(p);
    };
    runtime_.dormant = [](void* c, const RuntimePlayer& p) {
        return static_cast<EnginePlayers*>(c)->dormant(p);
    };
    runtime_.absolute_origin = [](void* c, const RuntimePlayer& p) {
        return static_cast<EnginePlayers*>(c)->origin(p);
    };
    runtime_.minimum_bounds = [](void* c, const RuntimePlayer& p) {
        return static_cast<EnginePlayers*>(c)->minimum(p);
    };
    runtime_.maximum_bounds = [](void* c, const RuntimePlayer& p) {
        return static_cast<EnginePlayers*>(c)->maximum(p);
    };
    runtime_.time = [](void* c) { return static_cast<EnginePlayers*>(c)->time(); };
}
std::ptrdiff_t EnginePlayers::offset(std::uint32_t pointer, std::uint32_t key,
                                     std::uint32_t mask) const {
    const auto& globals = bindings_.offsets.globals;
    return static_cast<std::ptrdiff_t>(*globals.pointers.at(pointer) ^ globals.keys.at(key) ^ mask);
}
RuntimePlayer* EnginePlayers::view(void* entity) {
    if (!entity)
        return nullptr;
    auto& result = players_[entity];
    result.engine_entity = entity;
    return &result;
}
RuntimePlayer* EnginePlayers::local() {
    return view(bindings_.local_player);
}
RuntimePlayer* EnginePlayers::by_index(std::int32_t i) {
    return view(engine::invoke<void*>(bindings_.entity_list, 3, i));
}
RuntimePlayer* EnginePlayers::by_handle(std::int32_t h) {
    return view(engine::invoke<void*>(bindings_.entity_list, 4, h));
}
RuntimeWeapon* EnginePlayers::weapon(std::int32_t handle) {
    auto* entity = engine::invoke<void*>(bindings_.entity_list, 4, handle);
    if (!entity)
        return nullptr;
    auto& result = weapons_[entity];
    result.engine_entity = entity;
    return &result;
}
PlayerRuntimeEngine& EnginePlayers::runtime_engine() {
    return runtime_;
}
bool EnginePlayers::is_player(const RuntimePlayer& p) const {
    return engine::invoke<bool>(p.engine_entity, 158);
}
bool EnginePlayers::is_weapon(const RuntimePlayer& p) const {
    return engine::invoke<bool>(p.engine_entity, 166);
}
std::uint16_t EnginePlayers::item_definition(const RuntimePlayer& p) const {
    return field<std::uint16_t>(p.engine_entity, offset(0x4346908c, 0x43468ba0, 0x5c9f0fbb));
}
std::int32_t EnginePlayers::owner_handle(const RuntimePlayer& p) const {
    return field<std::int32_t>(p.engine_entity, offset(0x434679d8, 0x43467fb4, 0x87907bc3));
}
std::int32_t EnginePlayers::local_index() const {
    return engine::invoke<std::int32_t>(bindings_.engine_client, 12);
}
bool EnginePlayers::dormant(const RuntimePlayer& p) const {
    return engine::invoke<bool>(static_cast<std::byte*>(p.engine_entity) + 8, 9);
}
std::int32_t EnginePlayers::index(const RuntimePlayer& p) const {
    return engine::invoke<std::int32_t>(static_cast<std::byte*>(p.engine_entity) + 8, 10);
}
Vector3 EnginePlayers::origin(const RuntimePlayer& p) const {
    return *engine::invoke<const Vector3*>(p.engine_entity, 10);
}
Vector3 EnginePlayers::minimum(const RuntimePlayer& p) const {
    return *engine::invoke<const Vector3*>(p.engine_entity, 76);
}
Vector3 EnginePlayers::maximum(const RuntimePlayer& p) const {
    return *engine::invoke<const Vector3*>(p.engine_entity, 77);
}
float EnginePlayers::time() const {
    return engine::invoke<float>(bindings_.engine_client, 14);
}
float EnginePlayers::tick_interval() const noexcept {
    return field<float>(bindings_.engine_globals, 0x20);
}
float EnginePlayers::frame_time() const noexcept {
    return field<float>(bindings_.engine_globals, 0x14);
}
float EnginePlayers::absolute_frame_time() const noexcept {
    return field<float>(bindings_.engine_globals, 8);
}
#define PLAYER_FIELD(TYPE, NAME, PTR, KEY, MASK)                                                   \
    TYPE EnginePlayers::NAME(const RuntimePlayer& p) const {                                       \
        return field<TYPE>(p.engine_entity, offset(PTR, KEY, MASK));                               \
    }
PLAYER_FIELD(std::uint8_t, life, 0x43468e78, 0x434684e4, 0x9d920e35)
PLAYER_FIELD(std::uint8_t, movement, 0x43468bd4, 0x4346a678, 0x7501cb91)
PLAYER_FIELD(std::int32_t, observer_mode, 0x4346a6cc, 0x43468ef8, 0x2f4e97c4)
PLAYER_FIELD(std::int32_t, observer_target, 0x43468f84, 0x43467e64, 0x3b704b41)
PLAYER_FIELD(std::int32_t, team, 0x434688b0, 0x4346a518, 0x8b5586f6)
PLAYER_FIELD(std::int32_t, squad, 0x434678ac, 0x43467b7c, 0x430c8993)
PLAYER_FIELD(std::uint8_t, eligibility, 0x43468bf8, 0x43467b8c, 0xd070684c)
PLAYER_FIELD(std::uint8_t, suppressed, 0x4346a5dc, 0x43468430, 0xcde290d2)
PLAYER_FIELD(Vector3, network_origin, 0x43468d30, 0x43468ed4, 0x2a671b42)
PLAYER_FIELD(Vector3, view_offset, 0x43468a34, 0x43468c00, 0xc246315e)
PLAYER_FIELD(std::int32_t, armor, 0x43467908, 0x434682e4, 0x47a6f50c)
PLAYER_FIELD(float, flash, 0x43467434, 0x43467fa8, 0x1bc6d160)
PLAYER_FIELD(float, duck_amount, 0x434679f8, 0x434675f8, 0x3309c0cf)
PLAYER_FIELD(float, duck_speed, 0x43467dd4, 0x4346a52c, 0xfb7e3404)
PLAYER_FIELD(float, simulation_time, 0x43469198, 0x4346a538, 0xd5fd2e2c)
PLAYER_FIELD(std::int32_t, active_weapon, 0x43467a74, 0x434678c0, 0xcd812654)
PLAYER_FIELD(std::int32_t, money, 0x43467a58, 0x4346a7f4, 0x6a00049e)
#undef PLAYER_FIELD
#define PLAYER_BOOL(NAME, PTR, KEY, MASK)                                                          \
    bool EnginePlayers::NAME(const RuntimePlayer& p) const {                                       \
        return field<std::uint8_t>(p.engine_entity, offset(PTR, KEY, MASK)) != 0;                  \
    }
PLAYER_BOOL(helmet, 0x43468c04, 0x43468fe8, 0x7d6280c7)
PLAYER_BOOL(scoped, 0x43468da4, 0x434684ec, 0xcacaeecc)
PLAYER_BOOL(vip, 0x43467c38, 0x43468498, 0xaf6689c2)
PLAYER_BOOL(defusing, 0x4346a7e0, 0x43468c34, 0xa15c3105)
#undef PLAYER_BOOL
float EnginePlayers::pose(const RuntimePlayer& p, unsigned index) const {
    return field<float>(p.engine_entity, offset(0x43467d78, 0x43467b80, 0x1caaf0a8) + index * 4);
}
std::int32_t EnginePlayers::inventory_weapon(const RuntimePlayer& p, unsigned index) const {
    return field<std::int32_t>(p.engine_entity,
                               offset(0x434685d4, 0x43467af4, 0xfc3297f7) + index * 4);
}
std::int32_t EnginePlayers::health(const RuntimePlayer& p) const {
    return engine::invoke<std::int32_t>(p.engine_entity, 122);
}
std::uint16_t EnginePlayers::definition(const RuntimeWeapon& w) const {
    return field<std::uint16_t>(w.engine_entity, offset(0x4346908c, 0x43468ba0, 0x5c9f0fbb));
}
std::int32_t EnginePlayers::ammunition(const RuntimeWeapon& w) const {
    return field<std::int32_t>(w.engine_entity, offset(0x4346a60c, 0x434678a8, 0xca5ec698));
}
bool EnginePlayers::pin_pulled(const RuntimeWeapon& w) const {
    return field<std::uint8_t>(w.engine_entity, offset(0x434670ec, 0x43467b1c, 0xe1df7e9b)) != 0;
}
float EnginePlayers::reload_cycle(const RuntimePlayer& p) const {
    const auto* descriptor =
        static_cast<const std::byte*>(p.engine_entity) + offset(0x4346a660, 0x434679e0, 0xb538bba5);
    if (field<std::int32_t>(descriptor, 0xc) < 1)
        return 1.0f;
    const auto* layer = field<const std::byte*>(descriptor, 0);
    if (field<float>(layer, 0x58) == 0.0f)
        return 1.0f;
    const auto sequence = field<std::int32_t>(layer, 0x50);
    const auto activity =
        bindings_.sequence_activity ? bindings_.sequence_activity(p.engine_entity, sequence) : -1;
    return activity == 0x3c7 ? field<float>(layer, 0x64) : 1.0f;
}
bool EnginePlayers::is_weapon(const RuntimeWeapon& w) const {
    return engine::invoke<bool>(w.engine_entity, 166);
}
WeaponInformation EnginePlayers::information(const RuntimeWeapon& w) const {
    const auto* info = information_pointer(w);
    return {field<float>(info, 0x100), field<float>(info, 0x10c), field<float>(info, 0x110),
            field<std::int32_t>(info, 0xfc)};
}
const void* EnginePlayers::information_pointer(const RuntimeWeapon& w) const {
    return engine::invoke<const void*>(w.engine_entity, 461);
}
float EnginePlayers::armor_ratio(const RuntimeWeapon& w) const {
    return engine::invoke<float>(w.engine_entity, 434);
}
std::int32_t EnginePlayers::damage(const RuntimeWeapon& w) const {
    return engine::invoke<std::int32_t>(w.engine_entity, 431);
}
std::int32_t EnginePlayers::capacity(const RuntimeWeapon& w) const {
    return engine::invoke<std::int32_t>(w.engine_entity, 375);
}
std::int32_t EnginePlayers::kind(const RuntimeWeapon& w) const {
    return engine::invoke<std::int32_t>(w.engine_entity, 455);
}
bool EnginePlayers::player_info(std::int32_t index, std::string_view& name) {
    if (!engine::invoke<bool>(bindings_.engine_client, 8, index, player_info_.data()))
        return false;
    name = {reinterpret_cast<const char*>(player_info_.data() + 0x10),
            hud::ObserverPlayerInfo{}.name.size()};
    name = name.substr(0, name.find('\0'));
    return true;
}
bool EnginePlayers::player_info(std::int32_t index, hud::ObserverPlayerInfo& output) {
    std::array<std::byte, 0x158> info{};
    if (!engine::invoke<bool>(bindings_.engine_client, 8, index, info.data()))
        return false;
    std::memcpy(output.name.data(), info.data() + 0x10, output.name.size());
    //4334E62B/639 test buffer+13D (HLTV) and+13C (fake player).
    output.fake_player = info[0x13c] != std::byte{};
    output.hltv = info[0x13d] != std::byte{};
    return true;
}
std::int32_t EnginePlayers::game_type() const {
    return convar(bindings_.game_type);
}
std::int32_t EnginePlayers::game_mode() const {
    return convar(bindings_.game_mode);
}
bool EnginePlayers::teammates_are_enemies() const {
    return convar(bindings_.teammates_are_enemies) != 0;
}
void* EnginePlayers::resource() const {
    return bindings_.player_resource ? *bindings_.player_resource : nullptr;
}
bool EnginePlayers::connected(const void* r, std::int32_t i) const {
    return field<std::uint8_t>(r, offset(0x43467cb8, 0x4346a814, 0x9571d5f5) + i) != 0;
}
bool EnginePlayers::alive(const void* r, std::int32_t i) const {
    return field<std::uint8_t>(r, offset(0x4346a724, 0x4346857c, 0xd266b537) + i) != 0;
}
std::int32_t EnginePlayers::resource_team(const void* r, std::int32_t i) const {
    return field<std::int32_t>(r, offset(0x434684f0, 0x4346a558, 0xc1c49a34) + i * 4);
}
std::int32_t EnginePlayers::resource_health(const void* r, std::int32_t i) const {
    return field<std::int32_t>(r, offset(0x4346716c, 0x434675b4, 0x9bb80163) + i * 4);
}
bool EnginePlayers::retain_dormant_alpha() const {
    return bindings_.game_rules && *bindings_.game_rules &&
           field<std::uint8_t>(*bindings_.game_rules, offset(0x43467910, 0x43467c08, 0xd720aea6)) !=
               0;
}
float EnginePlayers::rule_time() const {
    if (!bindings_.game_rules || !*bindings_.game_rules)
        return 0.0f;
    return field<float>(*bindings_.game_rules, offset(0x43468d6c, 0x43468428, 0x9f8e7f71));
}
std::int32_t EnginePlayers::bomb_carrier() const {
    if (!bindings_.game_rules || !*bindings_.game_rules)
        return -1;
    const auto* rules = *bindings_.game_rules;
    if (field<std::uint8_t>(rules, offset(0x4346746c, 0x43467d50, 0xd000a932)) ||
        field<std::uint8_t>(rules, offset(0x4346921c, 0x43468f24, 0x82f5ad08)))
        return -1;
    auto* resource = EnginePlayers::resource();
    if (!resource)
        return -1;
    return field<std::int32_t>(resource, offset(0x4346a370, 0x43468d70, 0x23b532ef));
}
bool EnginePlayers::spotted(const RuntimePlayer& p) const {
    return field<std::uint8_t>(p.engine_entity, offset(0x43467d48, 0x4346722c, 0xfb1e8dab)) != 0;
}
void EnginePlayers::set_spotted(const RuntimePlayer& p, SpottedProxy proxy) const {
    if (!proxy)
        return;
    SpottedProxyData packet;
    packet.words[2] = 1;
    packet.words[5] = 1;
    proxy(&packet, p.engine_entity,
          static_cast<std::byte*>(p.engine_entity) + offset(0x43467d48, 0x4346722c, 0xfb1e8dab));
}
void EnginePlayers::set_spotted(const RuntimePlayer& p) const {
    const auto* slot = bindings_.offsets.property_proxy_slots[2];
    const auto proxy = slot ? reinterpret_cast<SpottedProxy>(*slot) : nullptr;
    set_spotted(p, proxy);
}
} //namespace skeet::esp
