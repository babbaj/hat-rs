#pragma once

#include "vmread/hlapi/hlapi.h"
#include "il2cpp.h"

#include <string>
#include <algorithm>

inline std::string u16To8(const std::u16string& str) {
    std::string out; out.resize(str.size());
    std::transform(str.begin(), str.end(), out.begin(), [](uint16_t c) {
       return c <= 255 ? c : '?';
    });
    return out;
}

inline std::u16string readString(WinProcess& proc, pointer<rust::System_String_o> string) {
    const auto length = string.read(proc).m_stringLength;
    const auto chars = pointer<uint16_t>{&string.as_raw()->m_firstChar};

    std::u16string out;
    out.resize(length);
    proc.Read(chars.address, out.data(), length * sizeof(uint16_t));
    return out;
}

inline std::string readString8(WinProcess& proc, pointer<rust::System_String_o> string) {
    return u16To8(readString(proc, string));
}

