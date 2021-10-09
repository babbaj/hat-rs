#include <iostream>
#include <cstdio>
#include <thread>
#include <chrono>
#include <optional>

#include "radar.h"
#include "utils.h"

void debug(WinProcess& proc) {
    auto baseNetworkable = get_class<rust::BaseNetworkable_c>(proc);

    WinDll* const ga = proc.modules.GetModuleInfo("GameAssembly.dll");
    assert(ga);
    auto scannedAddress = scan_for_class(proc, *ga, "BaseNetworkable");
    assert(baseNetworkable.address == scannedAddress);
    assert(scannedAddress != 0);
    std::cout << std::hex << "BaseNetworkable = " << scannedAddress << '\n';
    printf("BaseNetworkable = %ld\n", scannedAddress);
}

// default = 2.5
pointer<float> getGravityPtr(WinProcess& proc, pointer<rust::BasePlayer_o> player) {
    pointer movement = getPlayerMovement(proc, player);
    return movement.member(gravityMultiplier);
}

void doSpider(WinProcess& proc, pointer<rust::BasePlayer_o> player) {
    pointer movement = getPlayerMovement(proc, player);
    movement.member(groundAngle).write(proc, 0.f);
    movement.member(groundAngleNew).write(proc, 0.f);
}

void fullbright(WinProcess& proc) {
    static pointer tod_sky_clazz = get_class<rust::TOD_Sky_c>(proc);
    if (tod_sky_clazz == nullptr) {
        return;
    }

    auto fields = tod_sky_clazz.member(static_fields).read(proc);
    pointer list = fields.read(proc).instances;

    std::vector instances = readList(proc, list);
    for (auto sky : instances) {
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

    gun->member(aimCone).write(proc, 0.f);
    gun->member(aimConePenaltyMax).write(proc, 0.f);
    gun->member(NoiseRadius).write(proc, 0.f);
}

void fatBullets(WinProcess& proc, pointer<rust::BasePlayer_o> player) {
    std::optional weapon = getHeldWeapon(proc, player);
    if (!weapon) return;

    pointer projectileList = weapon->member(createdProjectiles).read(proc);
    if (!projectileList) return;

    std::vector vec = readList(proc, projectileList);
    for (const auto& proj : vec) {
        proj.member(thickness).write(proc, 1.0f);
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
    //melee->member(maxDistance).write(proc, 10.f);
    melee->member(attackRadius).write(proc, 1.f);
    melee->member(blockSprintOnAttack).write(proc, false);
}

void instantEoka(WinProcess& proc, pointer<rust::BasePlayer_o> player) {
    std::optional eoka = getHeldT<rust::FlintStrikeWeapon_o, rust::FlintStrikeWeapon_c>(proc, player);
    if (!eoka) return;
    eoka->member(successFraction).write(proc, 1.f);
}

void hack_main(WinProcess& rust) {
    static_assert(alignof(rust::HeldEntity_o) == 8);
    puts("rust hack :DD");

    try {
        while (true) {
            auto test = getLocalPlayer(rust);
            if (test) {
                debug(rust);
                auto namePtr = test.member(_displayName).read(rust);
                std::string name = readString8(rust, namePtr);
                std::cout << "Local player name = " << name << '\n';
                float health = test.member(_health).read(rust);
                std::cout << "health = " << health << '\n';
                auto pos = getPosition(rust, test);
                std::cout << "Position = " << pos.x << ", " << pos.y << ", " << pos.z << '\n';
                auto className = getClassName(rust, test.member(klass).read(rust).address);
                std::cout << "Player class = " << className << '\n';
                auto players = getVisiblePlayers(rust);
                std::cout << players.size() << " players\n";

                std::thread radarThread([&] {
                    runRadar(rust);
                });
                std::thread fatBulletThread([&] {
                    while (true) {
                        auto player = getLocalPlayer(rust);
                        fatBullets(rust, player);

                        using namespace std::literals::chrono_literals;
                        std::this_thread::sleep_for(1ms);
                    }
                });
                while (true) {
                    auto local = getLocalPlayer(rust);

                    doSpider(rust, local);
                    fullbright(rust);
                    antiRecoil(rust, local);
                    fastBow(rust, local);
                    melee(rust, local);
                    instantEoka(rust, local);
                    //auto grav = getGravityPtr(rust, local);
                    //grav.write(rust, 0.1);

                    using namespace std::literals::chrono_literals;
                    std::this_thread::sleep_for(10ms);
                }
                radarThread.join();
                //fatBulletThread.join();
                continue;
            }

            using namespace std::literals::chrono_literals;
            std::this_thread::sleep_for(1000ms);
        }

    } catch (VMException& ex) {
        printf("Initialization error: %d\n", ex.value);
    }
}

pid_t getPid() {
    pid_t pid;
    FILE* pipe = popen("pidof qemu-system-x86_64", "r");
    fscanf(pipe, "%d", &pid);
    pclose(pipe);

    return pid;
}

#ifdef HACK_EXECUTABLE
int main() {
    auto ctx = WinContext(getPid());
    ctx.processList.Refresh();
    auto* rust = findRust(ctx.processList);

    hack_main(*rust);
}
#endif

#ifdef HACK_SHARED_LIBRARY
#include <dlfcn.h>
#include <SDL2/SDL_egl.h>
#include "overlay.h"


void swapBuffersHook() {

    static bool initialized = false;
    static WinProcess* rust;
    if (!initialized) {
        static auto ctx = WinContext(getPid());
        ctx.processList.Refresh();
        rust = findRust(ctx.processList);
        if (rust) {
            std::cout << "Found Rust with pid = " << rust->proc.pid << '\n';
            initialized = true;

            static std::thread hackThread([] {
                hack_main(*rust);
            });
        } else {
            std::cout << "Failed to find rust process\n";
            //std::terminate();
        }
    } else {
        renderOverlay(*rust);
    }
}

typedef EGLBoolean (*eglSwapBuffersWithDamageKHR_t)(EGLDisplay dpy,
        EGLSurface surface, const EGLint *rects, EGLint n_rects);

eglSwapBuffersWithDamageKHR_t khrImpl;
EGLBoolean hookKHR(EGLDisplay dpy, EGLSurface surface, const EGLint *rects, EGLint n_rects) {
    swapBuffersHook();
    return khrImpl(dpy, surface, rects, n_rects);
}

eglSwapBuffersWithDamageKHR_t extImpl;
EGLBoolean hookEXT(EGLDisplay dpy, EGLSurface surface, const EGLint *rects, EGLint n_rects) {
    swapBuffersHook();
    return extImpl(dpy, surface, rects, n_rects);
}

extern "C" void (* eglGetProcAddress(char const * procname))(void) {
    static auto *impl = (decltype(&eglGetProcAddress)) dlsym(RTLD_NEXT, "eglGetProcAddress");
    const auto proc = impl(procname);
    if (!proc) return nullptr;

    if (strcmp(procname, "eglSwapBuffersWithDamageKHR") == 0) {
        khrImpl = reinterpret_cast<eglSwapBuffersWithDamageKHR_t>(proc);
        return reinterpret_cast<void (*)()>(hookKHR);
    } else if (strcmp(procname, "eglSwapBuffersWithDamageEXT") == 0) {
        extImpl = reinterpret_cast<eglSwapBuffersWithDamageKHR_t>(proc);
        return reinterpret_cast<void (*)()>(hookEXT);
    } else {
        return proc;
    }
}

extern "C" EGLBoolean eglSwapBuffers(EGLDisplay dpy, EGLSurface surface) {
    static auto *swapbuffers = (decltype(&eglSwapBuffers)) dlsym(RTLD_NEXT, "eglSwapBuffers");
    swapBuffersHook();
    return swapbuffers(dpy, surface);
}
#endif