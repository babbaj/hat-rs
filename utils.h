#pragma once

#include <iostream>
#include <cstdio>
#include <cassert>

#include "vmread/hlapi/hlapi.h"
#include "il2cpp.h"
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

// apparently hat has the same thing
uint64_t scan_for_class0(WinProcess& proc, WinDll& gameAssembly, const char* name);

template<typename T>
inline pointer<T> scan_for_class(WinProcess& proc, WinDll& gameAssembly, const char* name) {
    return pointer<T>{scan_for_class0(proc, gameAssembly, name)};
}

template<typename T>
inline pointer<T> get_class(WinProcess& proc, const char* name) {
    static WinDll* const ga = proc.modules.GetModuleInfo("GameAssembly.dll");
    assert(ga);
    static const pointer<T> clazz = scan_for_class<T>(proc, *ga, name);
    return clazz;
}

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
    auto asObject = pointer<rust::Il2CppObject>{address};

    pointer<const char> className = asObject.read(proc).klass.read(proc)._1.name;
    std::string name = readCString(proc, className);
    return name;
}

inline std::vector<player> getVisiblePlayers(WinProcess& proc) {
    WinDll* ga = proc.modules.GetModuleInfo("GameAssembly.dll");
    assert(ga);

    static auto clazz = pointer<rust::BasePlayer_c>{scan_for_class0(proc, *ga, "BasePlayer")};
    rust::BufferList_TVal__o playerList = clazz
            .read(proc)
            .static_fields.read(proc)
            .visiblePlayerList.read(proc) // BasePlayer_StaticFields
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
    static WinDll* const ga = proc.modules.GetModuleInfo("GameAssembly.dll");
    assert(ga);

    static auto localPlayerClass = pointer<rust::LocalPlayer_c>{scan_for_class0(proc, *ga, "LocalPlayer")};
    auto staticFields = pointer<pointer<rust::LocalPlayer_StaticFields>>{&localPlayerClass.as_raw()->static_fields}
            .read(proc);

    return staticFields.read(proc)._Entity_k__BackingField;
}

inline std::pair<float, float> getAngles(WinProcess& proc, pointer<rust::BasePlayer_o> player) {
    pointer input = readMember(proc, player, &rust::BasePlayer_o::input);
    // not sure if that's actually the roll
    auto [yaw, pitch, roll] = readMember(proc, input, &rust::PlayerInput_o::bodyAngles);
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