#include <iostream>
#include <cstdio>
#include <thread>
#include <chrono>
#include <tuple>

#include "radar.h"
#include "utils.h"


/*void setNoRecoil(WinProcess& proc, pointer<rust::Item_o> item) {
    pointer ref = readMember(proc, item, &rust::Item_o::heldEntity).ent_cached;
    if (!ref) return;

    pointer cast = pointer<rust::BaseProjectile_o>{ref.address};
    pointer recoilProperties = readMember(proc, cast, &rust::BaseProjectile_o::recoil);
    if (!recoilProperties) return;


    rust::RecoilProperties_o recoil = recoilProperties.read(proc);
    recoil.recoilYawMin   = 0.f;
    recoil.recoilYawMax   = 0.f;
    recoil.recoilPitchMin = 0.f;
    recoil.recoilPitchMax = 0.f;
    recoilProperties.write(proc, recoil);
}

void noRecoil(WinProcess& proc, pointer<rust::BasePlayer_o> player) {
    auto activeId = readMember(proc, player, &rust::BasePlayer_o::clActiveItem);
    if (!activeId) return;

    pointer inv = readMember(proc, player, &rust::BasePlayer_o::inventory);
    pointer belt = readMember(proc, inv, &rust::PlayerInventory_o::containerBelt);
    auto itemList = readMember(proc, belt, &rust::ItemContainer_o::itemList).read(proc);
    pointer Item_array = pointer<rust::Item_array>{itemList._items.address}; // dumper mistakenly used protobuf
    pointer array = pointer<pointer<rust::Item_o>>{&Item_array.as_raw()->m_Items[0]};

    for (int i = 0; i < itemList._size; i++) {
        pointer item = array.read(proc, i);
        uint32_t uId = readMember(proc, item, &rust::Item_o::uid);
        if (activeId == uId) {
            setNoRecoil(proc, item);
        }
    }
}*/


int main() {
    std::cout << "sizeof (BasePlayer_o) = " << sizeof(rust::BasePlayer_o) << '\n';
    pid_t pid;
    FILE* pipe = popen("pidof qemu-system-x86_64", "r");
    fscanf(pipe, "%d", &pid);
    pclose(pipe);
    std::cout << "pid = " << pid << '\n';

    try {
        WinContext ctx(pid);
        ctx.processList.Refresh();

        auto* rust = findRust(ctx.processList);
        if (rust) {
            std::cout << "found rust\n";

            auto local = getLocalPlayer(*rust);
            if (local) {
                auto namePtr = readMember(*rust, local, &rust::BasePlayer_o::_displayName);
                std::string name = readString8(*rust, namePtr);
                std::cout << "Local player name = " << name << '\n';
                float health = readMember(*rust, local, &rust::BasePlayer_o::BaseCombatEntity__health);
                std::cout << "health = " << health << '\n';
                auto [x, y, z] = getPosition(*rust, local);
                std::cout << "Position = " << x << ", " << y << ", " << z << '\n';
                auto players = getVisiblePlayers(*rust);
                std::cout << players.size() << " players\n";

                runRadar(*rust);
            }
        } else {
            std::cout << "couldn't find rust\n";
        }
    } catch (VMException& ex) {
        printf("Initialization error: %d\n", ex.value);
    }

    return 0;
}
