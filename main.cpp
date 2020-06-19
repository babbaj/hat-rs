#include <iostream>
#include "vmread/hlapi/hlapi.h"
#include <cstdio>
#include <cassert>
#include "il2cpp.h"


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

int num_entites(WinProcess& proc) {
    WinDll* ga = proc.modules.GetModuleInfo("GameAssembly.dll");
    assert(ga);

    rust::BufferList_TVal__o objectList = pointer<rust::BaseNetworkable_c>{scan_for_class(proc, *ga, "BaseNetworkable")}
        .read(proc)
        .static_fields.read(proc)
        .clientEntities.read(proc) // BaseNetworkable_StaticFields
        .entityList.read(proc) // BaseNetworkable_EntityRealm_o
        .vals.read(proc); // ListDictionary_uint__BaseNetworkable__o

    auto array = pointer<pointer<rust::BaseNetworkable_o>>{(uintptr_t)&objectList.buffer.as_raw()->m_Items[0]};
    const auto obj_count = objectList.count;
    for (int i = 0; i < obj_count; i++) {
        pointer<rust::BaseNetworkable_o> obj_ptr = array.read(proc, i);
        if (!obj_ptr.address) {
            puts("!obj");
            continue;
        }
        auto obj = obj_ptr.read(proc);

        const intptr_t cachedPtr = obj.Object_m_CachedPtr;
        if (!cachedPtr) continue;

        auto asObject = pointer<rust::Il2CppObject>{(uint64_t)cachedPtr};

        std::string name = getClassName(proc, obj_ptr.address);
        std::cout << "Name = " << name << '\n';
    }
    return objectList.count;
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
            std::cout << num_entites(*rust) << '\n';
        } else {
            std::cout << "couldn't find rust\n";
        }
    } catch (VMException& ex) {
        printf("Initialization error: %d\n", ex.value);
    }

    return 0;
}
