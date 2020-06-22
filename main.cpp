#include <iostream>
#include "vmread/hlapi/hlapi.h"
#include <cstdio>
#include <cassert>
#include "il2cpp.h"
#include "csutils.h"
#include "native.h"


void printModules(WinProcess& process) {
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

WinProcess* findRust(WinProcessList& list) {
    return list.FindProcNoCase("rustclient.exe");
}

// apparently hat has the same thing
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

    for (uint64_t offset = data_size; offset > 0; offset -= 8) {
        char klass_name[0x100] = { 0 };
        auto klass = proc.Read<uint64_t>(base + data_header.VirtualAddress + offset);
        if (klass == 0) { continue; }
        auto name_pointer = proc.Read<uint64_t>(klass + 0x10);
        if (name_pointer == 0) { continue; }
        proc.Read(name_pointer, klass_name, sizeof(klass_name));
        if (!strcmp(klass_name, name)) {
            //printf("[*] 0x%x -> %s\n", data_header.VirtualAddress + offset, name);
            return klass;
        }
    }

    printf("[!] Unable to find %s in scan\n", name);
    exit(1);
}

uint64_t getModuleBase(WinProcess& proc, const char* name) {
    auto* module = proc.modules.GetModuleInfo(name);
    assert(module);
    return module->info.baseAddress;
}

// TODO: allow reading any size string
std::string readCString(WinProcess& proc, pointer<const char> str) {
    char buffer[256]{};
    proc.Read(str.address, buffer, sizeof(buffer));
    return std::string{buffer};
}

std::string getClassName(WinProcess& proc, uint64_t address) {
    auto asObject = pointer<rust::Il2CppObject>{address};

    pointer<const char> className = asObject.read(proc).klass.read(proc)._1.name;
    std::string name = readCString(proc, className);
    return name;
}

int num_players(WinProcess& proc) {
    WinDll* ga = proc.modules.GetModuleInfo("GameAssembly.dll");
    assert(ga);

    rust::BufferList_TVal__o playerList = pointer<rust::BasePlayer_c>{scan_for_class(proc, *ga, "BasePlayer")}
        .read(proc)
        .static_fields.read(proc)
        .visiblePlayerList.read(proc) // BasePlayer_StaticFields
        .vals.read(proc); // ListDictionary_ulong__BasePlayer__o

    auto array = pointer<pointer<rust::BaseNetworkable_o>>{(uintptr_t)&playerList.buffer.as_raw()->m_Items[0]};
    const auto obj_count = playerList.count;
    for (int i = 0; i < obj_count; i++) {
        pointer<rust::BaseNetworkable_o> obj_ptr = array.read(proc, i);
        if (!obj_ptr.address) {
            puts("!obj");
            continue;
        }
        auto obj = obj_ptr.read(proc);

    }
    return playerList.count;
}

pointer<rust::BasePlayer_o> getLocalPlayer(WinProcess& proc) {
    WinDll* ga = proc.modules.GetModuleInfo("GameAssembly.dll");
    assert(ga);

    auto localPlayerClass = pointer<rust::LocalPlayer_c>{scan_for_class(proc, *ga, "LocalPlayer")};
    auto staticFields = pointer<pointer<rust::LocalPlayer_StaticFields>>{&localPlayerClass.as_raw()->static_fields}
        .read(proc);

    return staticFields.read(proc)._Entity_k__BackingField;
}

Vector3 getPosition(WinProcess& proc, pointer<rust::BasePlayer_o> player) {
    // https://www.unknowncheats.me/forum/2562206-post1402.html
    // https://github.com/Dualisc/MalkovaEXTERNAL/blob/master/main.cc#L1079
    const uint64_t ptr = readMember(proc, player, &rust::BasePlayer_o::Object_m_CachedPtr);
    auto localPlayer  = proc.Read<uint64_t>(ptr + 0x30);
    auto localOC = proc.Read<uint64_t>(localPlayer  + 0x30);
    auto localT = proc.Read<uint64_t>(localOC + 0x8);
    auto localVS  = proc.Read<uint64_t>(localT + 0x38);

    return proc.Read<Vector3>(localVS  + 0x90);
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
            //std::cout << num_entites(*rust) << '\n';
            auto local = getLocalPlayer(*rust);
            if (local) {
                auto namePtr = readMember(*rust, local, &rust::BasePlayer_o::_displayName);
                std::string name = readString8(*rust, namePtr);
                std::cout << "Local player name = " << name << '\n';
                float health = readMember(*rust, local, &rust::BasePlayer_o::BaseCombatEntity__health);
                std::cout << "health = " << health << '\n';
                auto [x, y, z] = getPosition(*rust, local);
                std::cout << "Position = " << x << ", " << y << ", " << z << '\n';
                auto numPlayers = num_players(*rust);
                std::cout << numPlayers << " players\n";
            }
        } else {
            std::cout << "couldn't find rust\n";
        }
    } catch (VMException& ex) {
        printf("Initialization error: %d\n", ex.value);
    }

    return 0;
}
