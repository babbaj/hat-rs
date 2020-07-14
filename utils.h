#pragma once

#include <iostream>
#include <cstdio>
#include <cassert>

#include "vmread/hlapi/hlapi.h"
#include "csutils.h"
#include "wrappers.h"

inline void printModules(WinProcess& process) {
    PEB peb = process.GetPeb();
    short magic = process.Read<short>(peb.ImageBaseAddress);
    printf("\tBase:\t%lx\tMagic:\t%hx (valid: %hhx)\n", peb.ImageBaseAddress, magic, (char)(magic == IMAGE_DOS_SIGNATURE));

    printf("\tExports:\n");
    for (auto& o : process.modules) {
        printf("\t%.8lx\t%.8lx\t%lx\t%s\n", o.info.baseAddress, o.info.entryPoint, o.info.sizeOfModule, o.info.name);
        if (!strcmp("friendsui.DLL", o.info.name)) {
            for (auto& u : o.exports)
                printf("\t\t%lx\t%s\n", u.address, u.name);
        }
    }
}

inline WinProcess* findRust(WinProcessList& list) {
    return list.FindProcNoCase("rustclient.exe");
}

uint64_t scan_for_class(WinProcess& proc, WinDll& gameAssembly, const char* name);


inline uint64_t getModuleBase(WinProcess& proc, const char* name) {
    auto* module = proc.modules.GetModuleInfo(name);
    assert(module);
    return module->info.baseAddress;
}

// TODO: allow reading any size string
inline std::string readCString(WinProcess& proc, pointer<const char> str) {
    char buffer[256]{};
    proc.Read(str.address, buffer, sizeof(buffer));
    return std::string{buffer};
}

inline std::string getClassName(WinProcess& proc, uint64_t address) {
    auto asClass = pointer<rust::Il2CppClass>{address};

    pointer<const char> className = asClass.member(_1).member(name).read(proc);
    std::string name = readCString(proc, className);
    return name;
}

template<typename C>
inline pointer<C> get_class(WinProcess& proc) {
    static WinDll* const ga = proc.modules.GetModuleInfo("GameAssembly.dll");
    assert(ga);
    auto out = pointer<C>{proc.Read<uint64_t>(ga->info.baseAddress + C::offset)};

    /*auto scanned = scan_for_class(proc, *ga, C::name);

    auto scannedName = getClassName(proc, scanned);
    std::cout << "scanned name = " << scannedName << '\n';
    auto name1 = getClassName(proc, out.address);
    std::cout << "name = " << name1 << '\n';


    if (scanned != out.address) throw "bad";*/
    assert(out.address != 0);
    return out;
}

inline std::vector<player> getVisiblePlayers(WinProcess& proc) {
    WinDll* ga = proc.modules.GetModuleInfo("GameAssembly.dll");
    assert(ga);

    static auto clazzPtr = get_class<rust::BasePlayer_c>(proc);
    auto clazz = clazzPtr.read(proc);
    auto fields = clazz.static_fields.read(proc);
    rust::BufferList_TVal__o playerList =
            fields.visiblePlayerList.read(proc) // BasePlayer_StaticFields
            .vals.read(proc); // ListDictionary_ulong__BasePlayer__o

    auto array = pointer<pointer<rust::BasePlayer_o>>{(uintptr_t)&playerList.buffer.as_raw()->m_Items[0]};
    const auto obj_count = playerList.count;
    std::vector<player> out; out.reserve(obj_count);
    for (int i = 0; i < obj_count; i++) {
        pointer<rust::BasePlayer_o> obj_ptr = array.read(proc, i);
        if (!obj_ptr.address) {
            //puts("!obj");
            continue;
        }
        auto obj = obj_ptr.read(proc);
        if (obj.playerFlags & (int32_t)player_flags::Sleeping) {
            continue;
        }

        out.emplace_back(proc, obj_ptr, obj);
    }
    return out;
}

inline pointer<rust::BasePlayer_o> getLocalPlayer(WinProcess& proc) {
    static auto localPlayerClass = get_class<rust::LocalPlayer_c>(proc);

    //auto name = getClassName(proc, localPlayerClass.address);
    //std::cout << "local player class = " << name << '\n';
    auto staticFields = localPlayerClass.member(static_fields)//pointer<pointer<rust::LocalPlayer_StaticFields>>{&localPlayerClass.as_raw()->static_fields}
            .read(proc);

    return staticFields.read(proc)._Entity_k__BackingField;
}

inline std::pair<float, float> getAngles(WinProcess& proc, pointer<rust::BasePlayer_o> player) {
    pointer input = player.member(input).read(proc);
    // not sure if that's actually the roll
    auto [yaw, pitch, roll] = input.member(bodyAngles).read(proc);
    return {yaw, pitch};
}

template<typename List>
inline auto getListData(WinProcess& proc, pointer<List> list) {
    using T = typename std::decay_t<decltype(std::declval<List>()._items.read(proc).m_Items[0])>::type;

    List l = list.read(proc);
    const auto size = l._size;
    pointer itemsPtr = l._items;
    auto arrayPtr = pointer<pointer<T>>{(uint64_t)&itemsPtr.as_raw()->m_Items[0]};

    return std::pair<int32_t, pointer<pointer<T>>>{size, arrayPtr};
}

// this would be a really cute recursive function
inline bool is_super(WinProcess& proc, pointer<rust::Il2CppClass_1> super, pointer<rust::Il2CppClass_1> clazz) {
    assert(super != nullptr);
    pointer<rust::Il2CppClass_1> parent = super;
    do {
        if (clazz == parent) return true;
        parent = parent.member(parent).read(proc).member(_1);
    } while (parent != nullptr);
    return false;
}

template<typename S, typename C>
inline bool is_super(WinProcess& proc, pointer<S> super, pointer<C> clazz) {
    return is_super(proc, super.member(_1), clazz.member(_1));
}

template<typename S, typename C>
inline bool is_super_by_name(WinProcess& proc, pointer<C> clazz) {
    assert(clazz != nullptr);
    pointer<rust::Il2CppClass> parent = clazz.template unsafe_cast<rust::Il2CppClass>();
    do {
        // pretty inefficient tbh
        auto name = getClassName(proc, parent.address);
        if (name == S::name) return true;
        parent = parent.member(_1).member(parent).read(proc);
    } while (parent != nullptr);
    return false;
}