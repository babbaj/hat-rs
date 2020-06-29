#include <iostream>
#include <cstdio>
#include <thread>
#include <chrono>
#include <tuple>
#include <variant>

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
    auto activeId = readMember(proc, player, &rust::BasePlayer_o::clActiveItem);
    if (!activeId) return std::nullopt;

    pointer inv = readMember(proc, player, &rust::BasePlayer_o::inventory);
    pointer belt = readMember(proc, inv, &rust::PlayerInventory_o::containerBelt);
    auto itemList = readMember(proc, belt, &rust::ItemContainer_o::itemList);
    auto [size, array] = getListData(proc, itemList);

    for (int i = 0; i < size; i++) {
        pointer ptr = array.read(proc, i);
        auto item = pointer<rust::Item_o>{ptr.address}; // dumper mistakenly used protobuf Item

        uint32_t uId = readMember(proc, item, &rust::Item_o::uid);
        if (activeId == uId) {
            return {item};
        }
    }
    return std::nullopt;
}

bool isItemWeapon(WinProcess& proc, pointer<rust::Item_o> item) {
    auto definition = readMember(proc, item, &rust::Item_o::info);
    int32_t category = readMember(proc, definition, &rust::ItemDefinition_o::category);
    return category == (int)item_category::Weapon;
}

std::optional<pointer<rust::HeldEntity_o>> getHeldEntity(WinProcess& proc, pointer<rust::BasePlayer_o> player) {
    auto item = getHeldItem(proc, player);
    if (!item) return std::nullopt;

    pointer ref = readMember(proc, *item, &rust::Item_o::heldEntity).ent_cached;
    if (!ref) return std::nullopt;

    pointer cast = pointer<rust::HeldEntity_o>{ref.address};
    return cast;
}

// TODO: add more types
std::optional<std::variant<pointer<rust::BaseProjectile_o>, pointer<rust::BaseMelee_o>, pointer<rust::ThrownWeapon_o>, pointer<rust::FlameThrower_o>>>
asVariant(WinProcess& proc, pointer<rust::AttackEntity_o> heldEntity)
{
    const pointer class_projectile = get_class<rust::BaseProjectile_c>(proc,  "BaseProjectile");
    const pointer class_melee = get_class<rust::BaseMelee_c>(proc, "BaseMelee");
    const pointer class_thrown = get_class<rust::ThrownWeapon_c>(proc, "ThrownWeapon");
    const pointer class_flame = get_class<rust::FlameThrower_c>(proc, "FlameThrower");

    auto clazz = heldEntity.member(klass).read(proc);
    if (clazz == class_projectile) return {heldEntity.cast<rust::BaseProjectile_o>()};
    if (clazz == class_melee) return {heldEntity.cast<rust::BaseMelee_o>()};
    if (clazz == class_thrown) return {heldEntity.cast<rust::ThrownWeapon_o>()};
    if (clazz == class_flame) return {heldEntity.cast<rust::FlameThrower_o>()};
    return std::nullopt;
}

std::optional<pointer<rust::BaseProjectile_o>> getHeldGun(WinProcess& proc, pointer<rust::BasePlayer_o> player) {
    auto held = getHeldEntity(proc, player);
    if (!held) return std::nullopt;
    auto klass = held->member(klass).read(proc);
    auto cbp = get_class<rust::BaseProjectile_c>(proc, "BaseProjectile");
    if (klass != cbp) {
        return std::nullopt;
    } else {
        return {held->cast<rust::BaseProjectile_o>()};
    }
}

pointer<rust::PlayerWalkMovement_o> getPlayerMovement(WinProcess& proc, pointer<rust::BasePlayer_o> player) {
    return pointer<rust::PlayerWalkMovement_o>{readMember(proc, player, &rust::BasePlayer_o::movement).address};
}

// default = 2.5
pointer<float> getGravityPtr(WinProcess& proc, pointer<rust::BasePlayer_o> player) {
    pointer movement = getPlayerMovement(proc, player);
    return pointer<float>{&movement.as_raw()->gravityMultiplier};
}

void doSpider(WinProcess& proc, pointer<rust::BasePlayer_o> player) {
    pointer movement = getPlayerMovement(proc, player);
    writeMember(proc, movement, &rust::PlayerWalkMovement_o::groundAngle, 0.f);
    writeMember(proc, movement, &rust::PlayerWalkMovement_o::groundAngleNew, 0.f);
}

void fullbright(WinProcess& proc) {
    static WinDll* const ga = proc.modules.GetModuleInfo("GameAssembly.dll");
    assert(ga);
    static pointer tod_sky_clazz = pointer<rust::TOD_Sky_c>{scan_for_class0(proc, *ga, "TOD_Sky")};

    auto fields = readMember(proc, tod_sky_clazz, &rust::TOD_Sky_c::static_fields);
    pointer list = fields.read(proc).instances;
    auto [size, ptrArray] = getListData(proc, list);

    for (int i = 0; i < size; i++) {
        pointer<rust::TOD_Sky_o> sky = ptrArray.index(i).read(proc);
        writeMember(proc, sky, &rust::TOD_Sky_o::_IsDay_k__BackingField, true);

        pointer cycleParams = readMember(proc, sky, &rust::TOD_Sky_o::Cycle);
        writeMember(proc, cycleParams, &rust::TOD_CycleParameters_o::Hour, 12.f);
    }
}

void waterSpeed(WinProcess& proc, pointer<rust::BasePlayer_o> player) {
    writeMember(proc, player, &rust::BasePlayer_o::clothingWaterSpeedBonus, 1.f);
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
    pointer input = readMember(proc, player, &rust::BasePlayer_o::input);

    // set recoil to 0
    //writeMember(proc, input, &rust::PlayerInput_o::recoilAngles, rust::UnityEngine_Vector3_o{});
    input.member(recoilAngles).write(proc, rust::UnityEngine_Vector3_o{});
}

void fatBullets(WinProcess& proc, pointer<rust::BasePlayer_o> player) {
    std::optional weapon = getHeldGun(proc, player);
    if (!weapon) return;

    pointer projectileList = readMember(proc, *weapon, &rust::BaseProjectile_o::createdProjectiles);
    if (!projectileList) return;

    auto [numP, array] = getListData(proc, projectileList);
    for (int i = 0; i < numP; i++) {
        auto projectile = array.read(proc, i);
        writeMember(proc, projectile, &rust::Projectile_o::thickness, 1.5f);
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
                auto namePtr = readMember(*rust, test, &rust::BasePlayer_o::_displayName);
                std::string name = readString8(*rust, namePtr);
                std::cout << "Local player name = " << name << '\n';
                float health = readMember(*rust, test, &rust::BasePlayer_o::BaseCombatEntity__health);
                std::cout << "health = " << health << '\n';
                auto [x, y, z] = getPosition(*rust, test);
                std::cout << "Position = " << x << ", " << y << ", " << z << '\n';
                auto players = getVisiblePlayers(*rust);
                std::cout << players.size() << " players\n";

                std::thread radarThread([&] { runRadar(*rust); });
                std::thread fatBulletThread([&] {
                    while (true) {
                        auto player = getLocalPlayer(*rust);
                        fatBullets(*rust, player);
                    }
                });
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
                fatBulletThread.join();
            }
        } else {
            std::cout << "couldn't find rust\n";
        }
    } catch (VMException& ex) {
        printf("Initialization error: %d\n", ex.value);
    }

    return 0;
}
