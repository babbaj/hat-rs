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

std::optional<pointer<rust::Item_o>> getHeldItem(WinProcess& proc, pointer<rust::BasePlayer_o> player) {
    auto activeId = player.member(clActiveItem).read(proc);
    if (!activeId) return std::nullopt;
    static_assert(offsetof(rust::BasePlayer_o, clActiveItem) == 0x530);

    pointer inv = player.member(inventory).read(proc);
    pointer belt = inv.member(containerBelt).read(proc);
    auto itemList = belt.member(itemList).read(proc);
    auto [size, array] = getListData(proc, itemList);

    for (int i = 0; i < size; i++) {
        pointer ptr = array.read(proc, i);
        auto item = pointer<rust::Item_o>{ptr.address}; // dumper mistakenly used protobuf Item

        uint32_t uId = item.member(uid).read(proc);
        if (activeId == uId) {
            return {item};
        }
    }
    return std::nullopt;
}

bool isItemWeapon(WinProcess& proc, pointer<rust::Item_o> item) {
    auto definition = item.member(info).read(proc);
    int32_t category = definition.member(category).read(proc);
    return category == (int)item_category::Weapon;
}

bool isBaseProjectile(WinProcess& proc, pointer<rust::HeldEntity_o> heldEntity) {
    return is_super(proc, get_class<rust::BaseProjectile_c>(proc), heldEntity.member(klass).read(proc));
}

std::optional<pointer<rust::HeldEntity_o>> getHeldEntity(WinProcess& proc, pointer<rust::BasePlayer_o> player) {
    auto item = getHeldItem(proc, player);
    if (!item) return std::nullopt;

    pointer ref = item->member(heldEntity).read(proc).ent_cached;
    if (!ref) return std::nullopt;

    pointer cast = pointer<rust::HeldEntity_o>{ref.address};
    return cast;
}



std::optional<pointer<rust::BaseProjectile_o>> getHeldWeapon(WinProcess& proc, pointer<rust::BasePlayer_o> player) {
    auto held = getHeldEntity(proc, player);
    if (!held) return std::nullopt;
    if (isBaseProjectile(proc, *held)) {
        return {held->cast<rust::BaseProjectile_o>()};
    } else {
        return std::nullopt;
    }
}

pointer<rust::PlayerWalkMovement_o> getPlayerMovement(WinProcess& proc, pointer<rust::BasePlayer_o> player) {
    return pointer<rust::PlayerWalkMovement_o>{player.member(movement).read(proc).address};
}

// default = 2.5
pointer<float> getGravityPtr(WinProcess& proc, pointer<rust::BasePlayer_o> player) {
    pointer movement = getPlayerMovement(proc, player);
    return pointer<float>{&movement.as_raw()->gravityMultiplier};
}

void doSpider(WinProcess& proc, pointer<rust::BasePlayer_o> player) {
    pointer movement = getPlayerMovement(proc, player);
    movement.member(groundAngle).write(proc, 0.f);
    movement.member(groundAngleNew).write(proc, 0.f);
}

void fullbright(WinProcess& proc) {
    static pointer tod_sky_clazz = get_class<rust::TOD_Sky_c>(proc);

    auto fields = tod_sky_clazz.member(static_fields).read(proc);
    pointer list = fields.read(proc).instances;
    auto [size, ptrArray] = getListData(proc, list);

    for (int i = 0; i < size; i++) {
        pointer<rust::TOD_Sky_o> sky = ptrArray.index(i).read(proc);
        sky.member(_IsDay_k__BackingField).write(proc, true);

        pointer cycleParams = sky.member(Cycle).read(proc);
        cycleParams.member(Hour).write(proc, 12.f);
    }
}

void waterSpeed(WinProcess& proc, pointer<rust::BasePlayer_o> player) {
    player.member(clothingWaterSpeedBonus).write(proc, 1.f);
}

rust::UnityEngine_Vector3_o vecMinus(const rust::UnityEngine_Vector3_o& a, const rust::UnityEngine_Vector3_o& b) {
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

void Normalize(float& Yaw, float& Pitch) {
    if (Pitch < -89) Pitch = -89;
    else if (Pitch > 89) Pitch = 89;
    if (Yaw < -360) Yaw += 360;
    else if (Yaw > 360) Yaw -= 360;
}

void antiRecoil(WinProcess& proc, pointer<rust::BasePlayer_o> player) {
    pointer input = player.member(input).read(proc);

    // set recoil to 0
    input.member(recoilAngles).write(proc, rust::UnityEngine_Vector3_o{});
}

void fatBullets(WinProcess& proc, pointer<rust::BasePlayer_o> player) {
    std::optional weapon = getHeldWeapon(proc, player);
    if (!weapon) return;

    pointer projectileList = weapon->member(createdProjectiles).read(proc);
    if (!projectileList) return;

    auto [numP, array] = getListData(proc, projectileList);
    for (int i = 0; i < numP; i++) {
        auto projectile = array.read(proc, i);
        projectile.member(thickness).write(proc, 1.5f);
    }
}


int main() {
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

            auto test = getLocalPlayer(*rust);
            if (test) {
                auto namePtr = test.member(_displayName).read(*rust);
                std::string name = readString8(*rust, namePtr);
                std::cout << "Local player name = " << name << '\n';
                float health = test.member(_health).read(*rust);
                std::cout << "health = " << health << '\n';
                auto [x, y, z] = getPosition(*rust, test);
                std::cout << "Position = " << x << ", " << y << ", " << z << '\n';
                auto className = getClassName(*rust, test.member(klass).address);
                std::cout << "Player class = " << className << '\n';
                auto players = getVisiblePlayers(*rust);
                std::cout << players.size() << " players\n";

                std::thread radarThread([&] { runRadar(*rust); });
                /*std::thread fatBulletThread([&] {
                    while (true) {
                        auto player = getLocalPlayer(*rust);
                        fatBullets(*rust, player);
                    }
                });*/
                while (true) {
                    using namespace std::literals::chrono_literals;
                    auto local = getLocalPlayer(*rust);

                    doSpider(*rust, local);
                    fullbright(*rust);
                    antiRecoil(*rust, local);
                    //auto grav = getGravityPtr(*rust, local);
                    //grav.write(*rust, 1.5);

                    std::this_thread::sleep_for(1ms);
                }
                radarThread.join();
                //fatBulletThread.join();
            }
        } else {
            std::cout << "couldn't find rust\n";
        }
    } catch (VMException& ex) {
        printf("Initialization error: %d\n", ex.value);
    }

    return 0;
}
