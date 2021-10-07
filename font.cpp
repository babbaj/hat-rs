#define STB_TRUETYPE_IMPLEMENTATION
#include "font.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb/stb_image_write.h>

#include <fstream>
#include <iostream>
#include <cstdint>

static std::vector<unsigned char> readFile(const char* path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<unsigned char> buffer(size);
    if (file.read(reinterpret_cast<char *>(buffer.data()), size)) {
        return buffer;
    } else {
        throw std::runtime_error("failed to read font");
    }
}

SimpleFont initFont2() {
    auto fontData = readFile("arial.ttf");

    unsigned char temp_bitmap[512*512]{};
    auto cdata = std::make_unique<std::array<stbtt_bakedchar, 96>>();
    stbtt_BakeFontBitmap(fontData.data(), 0, 32.0, temp_bitmap,512,512, 32,96, cdata->data()); // no guarantee this fits!
    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, 512,512, 0, GL_ALPHA, GL_UNSIGNED_BYTE, temp_bitmap);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    return {
        .texture = tex,
        .cdata = std::move(cdata)
    };
}

Font initFont() {
    auto fontData = readFile("arial.ttf");

    stbtt_fontinfo info;
    if (!stbtt_InitFont(&info, reinterpret_cast<const unsigned char *>(fontData.data()), 0)) {
        printf("stbtt_InitFont failed\n");
        exit(1);
    }


    std::array<uint8_t, 1024 * 1024> atlasData{};
    std::array<stbtt_packedchar, 96> cdata{};

    stbtt_pack_context context;
    if (!stbtt_PackBegin(&context, atlasData.data(), 1024 /*atlasW*/, 1024 /*atlasH*/, 0, 1, nullptr)) {
        std::cerr << "Failed to initialize font" << std::endl;
        exit(1);
    }

    stbtt_PackSetOversampling(&context, 2, 2); // oversample x/y
    if (!stbtt_PackFontRange(&context, fontData.data(), 0, 40, ' ', 96, cdata.data())) {
        std::cerr << "Failed to pack font" << std::endl;
        exit(1);
    }

    stbtt_PackEnd(&context);

    std::fill(atlasData.begin(), atlasData.end(), 0xFF);

    GLuint texId;
    glGenTextures(1, &texId);
    glBindTexture(GL_TEXTURE_2D, texId);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 1024, 1024, 0, GL_RED, GL_UNSIGNED_BYTE, atlasData.data());
    glHint(GL_GENERATE_MIPMAP_HINT, GL_NICEST);
    glGenerateMipmap(GL_TEXTURE_2D);

    stbi_write_png("out.png", 1024, 1024, 1, atlasData.data(), 1024);

    return {
        .info = info,
        .fileData = std::move(fontData),
        .atlasW = 1024,
        .atlasH = 1024,
        .atlas = {atlasData.begin(), atlasData.end()},
        .cdata = cdata,
        .texture = texId
    };
}

