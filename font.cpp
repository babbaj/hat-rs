#define STB_TRUETYPE_IMPLEMENTATION
#include "font.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb/stb_image_write.h>

#include <fstream>
#include <iostream>
#include <cstdint>
#include <SDL_opengl.h>

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


Font initFont() {
    auto fontData = readFile("arial.ttf");

    stbtt_fontinfo info;
    if (!stbtt_InitFont(&info, reinterpret_cast<const unsigned char *>(fontData.data()), 0)) {
        printf("stbtt_InitFont failed\n");
        exit(1);
    }

    std::array<uint8_t, ATLAS_DIM * ATLAS_DIM> atlasData{};
    std::array<stbtt_packedchar, 96> cdata{};

    stbtt_pack_context context;
    if (!stbtt_PackBegin(&context, atlasData.data(), ATLAS_DIM /*atlasW*/, ATLAS_DIM /*atlasH*/, 0, 1, nullptr)) {
        std::cerr << "Failed to initialize font" << std::endl;
        exit(1);
    }

    stbtt_PackSetOversampling(&context, 2, 2); // oversample x/y
    if (!stbtt_PackFontRange(&context, fontData.data(), 0, 40, ' ', 96, cdata.data())) {
        std::cerr << "Failed to pack font" << std::endl;
        exit(1);
    }

    stbtt_PackEnd(&context);

    //std::fill(atlasData.begin(), atlasData.end(), 0xFF);
    stbi_write_png("out.png", ATLAS_DIM, ATLAS_DIM, 1, atlasData.data(), ATLAS_DIM);


    void printError();
    printError();

    GLuint texId;
    glGenTextures(1, &texId);
    glBindTexture(GL_TEXTURE_2D, texId);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, ATLAS_DIM, ATLAS_DIM, 0, GL_RED, GL_UNSIGNED_BYTE, atlasData.data());
    glHint(GL_GENERATE_MIPMAP_HINT, GL_NICEST);
    glGenerateMipmap(GL_TEXTURE_2D);

    //GLuint texId;
    //glGenTextures(1, &texId);
    //glBindTexture(GL_TEXTURE_2D, texId);
    //glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    //glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, ATLAS_DIM, ATLAS_DIM, 0, GL_RED, GL_UNSIGNED_BYTE, atlasData.data());
    //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    //std::array<uint8_t, ATLAS_DIM * ATLAS_DIM> buffer{};
    //glBindTexture(GL_TEXTURE_2D, texId);
    //glGetTexImage(GL_TEXTURE_2D, 0, GL_RED, GL_UNSIGNED_BYTE, buffer.data());
    //stbi_write_png("nword.png", ATLAS_DIM, ATLAS_DIM, 1, atlasData.data(), ATLAS_DIM);

    //exit(0);
    return {
        .info = info,
        .fileData = std::move(fontData),
        .atlasW = ATLAS_DIM,
        .atlasH = ATLAS_DIM,
        .atlas = {atlasData.begin(), atlasData.end()},
        .cdata = cdata,
        .texture = texId
    };
}

