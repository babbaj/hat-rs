#pragma once

#include <string>

#include "vmread/hlapi/hlapi.h"
#include "il2cpp.h"
#include "csutils.h"

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

struct vector3 {
    float x, y, z;
};

vector3 getPosition(WinProcess& proc, uint64_t m_cachedPtr) {
    // https://www.unknowncheats.me/forum/2562206-post1402.html
    // https://github.com/Dualisc/MalkovaEXTERNAL/blob/master/main.cc#L1079
    auto localPlayer  = proc.Read<uint64_t>(m_cachedPtr + 0x30);
    auto localOC = proc.Read<uint64_t>(localPlayer  + 0x30);
    auto localT = proc.Read<uint64_t>(localOC + 0x8);
    auto localVS  = proc.Read<uint64_t>(localT + 0x38);

    return proc.Read<vector3>(localVS  + 0x90);
}

vector3 getPosition(WinProcess& proc, pointer<rust::BasePlayer_o> player) {
    const uint64_t ptr = readMember(proc, player, &rust::BasePlayer_o::Object_m_CachedPtr);
    return getPosition(proc, ptr);
}

struct player {
    std::string name;
    float health;
    vector3 position;

    explicit player(WinProcess& proc, const rust::BasePlayer_o& player) {
        this->name = readString8(proc, player._displayName);
        this->health = player.BaseCombatEntity__health;
        this->position = getPosition(proc, player.Object_m_CachedPtr);
    }

    explicit player(WinProcess& proc, pointer<rust::BasePlayer_o> pPtr): player(proc, pPtr.read(proc)) { }
};