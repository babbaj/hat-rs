#include <iostream>
#include <cstdio>
#include <cassert>

#include "../vmread/hlapi/hlapi.h"

#include "utils.h"


uint64_t scan_for_class(WinProcess& proc, WinDll& gameAssembly, const char* name)
{
    uint64_t base = gameAssembly.info.baseAddress;
    auto dos_header = proc.Read<IMAGE_DOS_HEADER>(base);
    auto data_header = proc.Read<IMAGE_SECTION_HEADER>(base + dos_header.e_lfanew + sizeof(IMAGE_NT_HEADERS64) + (3 * 40));
    auto next_section = proc.Read<IMAGE_SECTION_HEADER>(base + dos_header.e_lfanew + sizeof(IMAGE_NT_HEADERS64) + (4 * 40));
    auto data_size = next_section.VirtualAddress - data_header.VirtualAddress;

    if (strcmp((char*)data_header.Name, ".data")) {
        printf("[!] Section order changed\n");
    }

    const auto begin = base + data_header.VirtualAddress;
    const auto end = begin + data_size;
    for (uint64_t offset = data_size; offset > 0; offset -= 8) {
        char klass_name[0x100] = { 0 };
        auto klass = proc.Read<uint64_t>(base + data_header.VirtualAddress + offset);
        if (klass == 0) { continue; }
        auto name_pointer = proc.Read<uint64_t>(klass + 0x10);
        if (name_pointer == 0) { continue; }
        proc.Read(name_pointer, klass_name, sizeof(klass_name));
        if (!strcmp(klass_name, name)) {
            printf("[*] 0x%x -> %s\n", data_header.VirtualAddress + offset, name);
            return klass;
        }
    }

    printf("[!] Unable to find %s in scan\n", name);
    throw "reeeeeee";
    exit(1);
}

// TODO: cache subclasses?
bool isNpc(WinProcess& proc, const rust::BasePlayer_o& player) {
    //static auto npcClass = get_class<rust::HumanNPCNew_c>(proc);
    //auto clazz = player.klass;
    //return is_super(proc, npcClass, clazz);
    auto clazz = player.klass;
    return is_super_by_name<rust::HumanNPCNew_c>(proc, clazz);
}


std::vector<player> getVisiblePlayers(WinProcess& proc) {
    static WinDll* ga = proc.modules.GetModuleInfo("GameAssembly.dll");
    assert(ga);

    auto clazzPtr = get_class<rust::BasePlayer_c>(proc);
    if (clazzPtr == nullptr) {
        return {};
    }
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

        if (obj.playerFlags & (int32_t)player_flags::Sleeping && !isNpc(proc, obj)) {
            continue;
        }

        out.emplace_back(proc, obj_ptr, obj);
    }
    return out;
}

pointer<rust::BasePlayer_o> getLocalPlayer(WinProcess& proc) {
    static const auto localPlayerClass = get_class<rust::LocalPlayer_c>(proc);
    if (!localPlayerClass) {
        puts("trolled");
        exit(1);
    }

    //auto name = getClassName(proc, localPlayerClass.address);
    //std::cout << "local player class = " << name << '\n';
    static const auto staticFields = localPlayerClass.member(static_fields)
            .read(proc);

    return staticFields.read(proc)._Entity_k__BackingField;
}

glm::vec2 getAngles(WinProcess& proc, pointer<rust::BasePlayer_o> player) {
    pointer input = player.member(input).read(proc);
    // not sure if that's actually the roll
    auto [pitch, yaw, roll] = input.member(bodyAngles).read(proc);
    return {pitch, yaw};
}

std::optional<pointer<rust::Item_o>> getHeldItem(WinProcess& proc, pointer<rust::BasePlayer_o> player) {
    auto activeId = player.member(clActiveItem).read(proc);
    if (!activeId) return std::nullopt;

    pointer inv = player.member(inventory).read(proc);
    pointer belt = inv.member(containerBelt).read(proc);
    auto itemList = belt.member(itemList).read(proc);
    auto [size, array] = getListData(proc, itemList);

    for (int i = 0; i < size; i++) {
        pointer ptr = array.read(proc, i);
        auto item = ptr.as_unchecked<rust::Item_o>();//pointer<rust::Item_o>{ptr.address}; // dumper mistakenly used protobuf Item

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

    return ref.cast<rust::HeldEntity_o>();
}