#include <iostream>
#include <cstdio>
#include <cassert>
#include <vector>
#include <algorithm>

#include "../vmread/hlapi/hlapi.h"

#include "utils.h"
#include "windoze.h"


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
    return is_super_by_name<rust::NPCPlayer_c>(proc, clazz);
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

    auto array = playerList.buffer.member(m_Items).unsafe_cast<pointer<rust::BasePlayer_o>>();
    const auto obj_count = playerList.count;
    std::vector<player> out; out.reserve(obj_count);
    for (int i = 0; i < obj_count; i++) {
        pointer<rust::BasePlayer_o> obj_ptr = array.index(i).read(proc);
        if (!obj_ptr.address) {
            //puts("!obj");
            continue;
        }
        auto obj = obj_ptr.read(proc);

        if ((obj.playerFlags & (int32_t)player_flags::Sleeping) || isNpc(proc, obj)) {
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
        auto item = ptr.unsafe_cast<rust::Item_o>(); // dumper mistakenly used protobuf Item

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

// https://github.com/furryhater2008/pm_fix/blob/937ac5b0b2247d99b4c6e7520a05696e85c17a83/plusminus/stuff/il2cpp.h#L457
uint64_t getUnityCamera(WinProcess& proc) {
    static const uint64_t base = proc.modules.GetModuleInfo("UnityPlayer.dll")->info.baseAddress;
    assert(base);
    const auto dos_header = pointer<win::IMAGE_DOS_HEADER>{base}.read(proc);
    const auto nt_header = pointer<win::IMAGE_NT_HEADERS64>{base + dos_header.e_lfanew}.read(proc);
    uint64_t data_base;
    uint64_t data_size;
    for (int i = 0;;) {
        const auto section_header = pointer<win::IMAGE_SECTION_HEADER>{
                base + dos_header.e_lfanew + // nt_header base
                sizeof(win::IMAGE_NT_HEADERS64) +  // start of section headers
                (i * sizeof(win::IMAGE_SECTION_HEADER))
        }.read(proc); // section header at our index
        std::string name = section_header.Name;
        if (strcmp((char *) section_header.Name, ".data") == 0) {
            data_base = section_header.VirtualAddress + base;
            data_size = section_header.SizeOfRawData;
            break;
        }
        i++;
        if (i >= nt_header.FileHeader.NumberOfSections) {
            puts("failed to find .data");
            exit(1);
        }
    }
    std::vector<std::byte> section_data(data_size);
    proc.Read(data_base, section_data.data(), data_size);
    const auto needle = "AllCameras";
    const auto camera_string = std::search(section_data.data(), section_data.data() + section_data.size(),
                                           (std::byte *) needle, (std::byte *) needle + strlen(needle));
    if (camera_string == section_data.data() + section_data.size()) {
        puts("failed to find camera string");
        exit(1);
    }
    for (auto walker = (uint64_t *) camera_string; (uint64_t) walker > 0; walker -= 1) {
        // this loks pretty goofy
        if (*walker > 0x100000 && *walker < 0xF00000000000000) {
            // [[[[unityplayer.dll + ctable offset]]] + 0x30] = Camera
            return *walker;
        }
    }
    puts("failed to find cameras");
    exit(1);
}
