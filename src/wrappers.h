#pragma once

#include <string>
#include <optional>

#include <glm/vec3.hpp>
#include <glm/vec2.hpp>
#include "../vmread/hlapi/hlapi.h"
#include "../sdk/il2cpp.h"

#include "csutils.h"
//#include "utils.h"
#include "pointer.hpp"


enum class player_flags: int32_t {
    Unused1 = 1,
    Unused2 = 2,
    IsAdmin = 4,
    ReceivingSnapshot = 8,
    Sleeping = 16,
    Spectating = 32,
    Wounded = 64,
    IsDeveloper = 128,
    Connected = 256,
    ThirdPersonViewmode = 1024,
    EyesViewmode = 2048,
    ChatMute = 4096,
    NoSprint = 8192,
    Aiming = 16384,
    DisplaySash = 32768,
    Relaxed = 65536,
    SafeZone = 131072,
    ServerFall = 262144,
    Workbench1 = 1048576,
    Workbench2 = 2097152,
    Workbench3 = 4194304
};

enum class item_category : int {
    Weapon = 0,
    Construction = 1,
    Items = 2,
    Resources = 3,
    Attire = 4,
    Tool = 5,
    Medical = 6,
    Food = 7,
    Ammunition = 8,
    Traps = 9,
    Misc = 10,
    All = 11,
    Common = 12,
    Component = 13,
    Search = 14,
    Favourite = 15,
    Electrical = 16,
    Fun = 17
};


inline glm::vec3 getPosition(WinProcess& proc, uint64_t m_cachedPtr) {
    // https://www.unknowncheats.me/forum/2562206-post1402.html
    // https://github.com/Dualisc/MalkovaEXTERNAL/blob/master/main.cc#L1079
    auto localPlayer  = proc.Read<uint64_t>(m_cachedPtr + 0x30);
    auto localOC = proc.Read<uint64_t>(localPlayer  + 0x30);
    auto localT = proc.Read<uint64_t>(localOC + 0x8);
    auto localVS  = proc.Read<uint64_t>(localT + 0x38);

    return proc.Read<glm::vec3>(localVS  + 0x90);
}

inline glm::vec3 getPosition(WinProcess& proc, pointer<rust::BasePlayer_o> player) {
    const uint64_t ptr = player.member(m_CachedPtr).read(proc);
    return getPosition(proc, ptr);
}

struct player {
    pointer<rust::BasePlayer_o> handle;
    std::string name;
    float health;
    glm::vec3 position;
    glm::vec2 angles; // pitch/yaw
    uint32_t flags; // playerFlags
    std::optional<std::string> weaponName;

    [[nodiscard]] bool isSleeping() const {
        return (this->flags & (int32_t)player_flags::Sleeping) != 0;
    }

    explicit player(WinProcess& proc, pointer<rust::BasePlayer_o> handle_, const rust::BasePlayer_o& player);

    explicit player(WinProcess& proc, pointer<rust::BasePlayer_o> pPtr): player(proc, pPtr, pPtr.read(proc)) { }
};