#pragma once

#include <string_view>
#include <vector>
#include <array>
#include <utility>
#include <memory>

#include <stb/stb_truetype.h>
#include <GLES3/gl3.h>

struct Font {
    stbtt_fontinfo info;
    std::vector<unsigned char> fileData;
    int atlasW;
    int atlasH;
    std::vector<uint8_t> atlas;
    std::array<stbtt_packedchar, 96> cdata;
    GLuint texture;
};

struct SimpleFont {
    GLuint texture;
    std::unique_ptr<std::array<stbtt_bakedchar, 96>> cdata;
};

SimpleFont initFont2();

Font initFont();

std::pair<int, int> stringDimensions(std::string_view);