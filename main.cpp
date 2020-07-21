#include <iostream>
#include <cstdio>
#include <thread>
#include <chrono>
#include <tuple>

#include "radar.h"
#include "utils.h"

#include <SDL2/SDL.h>


void debug(WinProcess& proc) {
    auto baseNetworkable = get_class<rust::BaseNetworkable_c>(proc);

    WinDll* const ga = proc.modules.GetModuleInfo("GameAssembly.dll");
    assert(ga);
    auto scannedAddress = scan_for_class(proc, *ga, "BaseNetworkable");
    assert(baseNetworkable.address == scannedAddress);
}

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
}*/

std::optional<pointer<rust::Item_o>> getHeldItem(WinProcess& proc, pointer<rust::BasePlayer_o> player) {
    auto activeId = player.member(clActiveItem).read(proc);
    if (!activeId) return std::nullopt;

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
    auto clazz = heldEntity.member(klass).read(proc);
    return is_super_by_name<rust::BaseProjectile_c, decltype(clazz)::type>(proc, clazz);
}

std::optional<pointer<rust::HeldEntity_o>> getHeldEntity(WinProcess& proc, pointer<rust::BasePlayer_o> player) {
    auto item = getHeldItem(proc, player);
    if (!item) return std::nullopt;

    pointer ref = item->member(heldEntity).read(proc).ent_cached;
    if (!ref) return std::nullopt;

    pointer cast = pointer<rust::HeldEntity_o>{ref.address};
    return cast;
}

template<typename T, typename C> // TODO: get rid of C parameter
std::optional<pointer<T>> getHeldT(WinProcess& proc, pointer<rust::BasePlayer_o> player) {
    static_assert(std::is_base_of_v<rust::HeldEntity_o, T>);

    auto held = getHeldEntity(proc, player);
    if (!held) return std::nullopt;
    auto clazz = held->member(klass).read(proc);
    bool isType = is_super_by_name<C, typename decltype(clazz)::type>(proc, clazz);

    if (isType) {
        return {held->cast<T>()};
    } else {
        return std::nullopt;
    }
}

std::optional<pointer<rust::BaseProjectile_o>> getHeldWeapon(WinProcess& proc, pointer<rust::BasePlayer_o> player) {
    return getHeldT<rust::BaseProjectile_o, rust::BaseProjectile_c>(proc, player);
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
    std::optional gun = getHeldWeapon(proc, player);
    if (!gun) return;

    pointer recoilPtr = gun->member(recoil).read(proc);
    if (!recoilPtr) return;

    rust::RecoilProperties_o recoil = recoilPtr.read(proc);
    recoil.recoilYawMin   = 0.f;
    recoil.recoilYawMax   = 0.f;
    recoil.recoilPitchMin = 0.f;
    recoil.recoilPitchMax = 0.f;
    recoilPtr.write(proc, recoil);
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

std::optional<pointer<rust::BowWeapon_o>> getBow(WinProcess& proc, pointer<rust::BasePlayer_o> player) {
    return getHeldT<rust::BowWeapon_o, rust::BowWeapon_c>(proc, player);
}

void fastBow(WinProcess& proc, pointer<rust::BasePlayer_o> player) {
    std::optional bow = getBow(proc, player);
    if (!bow) return;
    bow->member(attackReady).write(proc, true);
    bow->member(arrowBack).write(proc, 1.f);
}

void melee(WinProcess& proc, pointer<rust::BasePlayer_o> player) {
    std::optional melee = getHeldT<rust::BaseMelee_o, rust::BaseMelee_c>(proc, player);
    if (!melee) return;
    melee->member(maxDistance).write(proc, 10.f);
    melee->member(attackRadius).write(proc, 1.f);
    melee->member(blockSprintOnAttack).write(proc, false);
}

void eokaLuck(WinProcess& proc, pointer<rust::BasePlayer_o> player) {
    std::optional eoka = getHeldT<rust::FlintStrikeWeapon_o, rust::FlintStrikeWeapon_c>(proc, player);
    if (!eoka) return;
    eoka->member(successFraction).write(proc, 1.f);
}

void hack_main() {
    static_assert(alignof(rust::HeldEntity_o) == 8);
    static_assert(offsetof(rust::BaseProjectile_o, ownerItemUID) == 0x1D0);
    static_assert(offsetof(rust::BaseProjectile_o, deployDelay) == 0x1D8);

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

            debug(*rust);
            auto test = getLocalPlayer(*rust);
            if (test) {
                auto namePtr = test.member(_displayName).read(*rust);
                std::string name = readString8(*rust, namePtr);
                std::cout << "Local player name = " << name << '\n';
                float health = test.member(_health).read(*rust);
                std::cout << "health = " << health << '\n';
                auto [x, y, z] = getPosition(*rust, test);
                std::cout << "Position = " << x << ", " << y << ", " << z << '\n';
                auto className = getClassName(*rust, test.member(klass).read(*rust).address);
                std::cout << "Player class = " << className << '\n';
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
                    fastBow(*rust, local);
                    melee(*rust, local);
                    eokaLuck(*rust, local);
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
}

#ifdef HACK_EXECUTABLE
int main() {
    hack_main();
}
#endif

#ifdef HACK_SHARED_LIBRARY
#include <dlfcn.h>
#include <SDL2/SDL_egl.h>
#include "overlay.h"

extern "C" EGLBoolean eglSwapBuffers(EGLDisplay dpy, EGLSurface surface) {
    static auto *swapbuffers = (decltype(&eglSwapBuffers)) dlsym(RTLD_NEXT, "eglSwapBuffers");

    static std::thread hackThread([] {
       hack_main();
    });
    renderOverlay(dpy, surface);

    return swapbuffers(dpy, surface);
}
#endif