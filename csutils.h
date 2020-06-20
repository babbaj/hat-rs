#pragma once

#include "vmread/hlapi/hlapi.h"
#include "il2cpp.h"

#include <string>
#include <codecvt>
#include <locale>

std::string u16To8(const std::u16string& str) {
    std::wstring_convert<std::codecvt_utf8<char16_t>, char16_t> convert;
    return convert.to_bytes(str.c_str());
}

// haven't tested this yet
std::u16string readString(WinProcess& proc, pointer<rust::System_String_o> string) {
    const auto length = string.read(proc).m_stringLength;
    const auto chars = pointer<uint16_t>{&string.as_raw()->m_firstChar};

    std::u16string out;
    out.resize(length);
    proc.Read(chars.address, out.data(), length * sizeof(uint16_t));
    return out;
}