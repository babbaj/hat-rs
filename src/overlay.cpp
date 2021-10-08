#include <iostream>
#include <vector>
#include <optional>
#include <array>

#include "utils.h"
#include "overlay.h"
#include "font.h"

#define GL_GLEXT_PROTOTYPES
#include <SDL2/SDL_opengl.h>
#include <SDL2/SDL.h>

#include <glm/vec3.hpp>
#include <glm/vec2.hpp>
#include <glm/mat4x4.hpp>
#include <glm/ext.hpp>

#include <fmt/core.h>

void printError() {
    auto error = glGetError();
    if (error != GL_NO_ERROR) {
        std::cout << "error = " << error << std::endl;
    } else {
        std::cout << "no error" << std::endl;
    }
}

void flushErrors() {
    GLuint err;
    while ((err = glGetError()) != GL_NO_ERROR);
}

GLuint LoadShaders(const char* vertexShaderCode, const char* fragmentShaderCode);

[[deprecated]] glm::mat4 getViewMatrix(WinProcess& rust) {
    constexpr auto gom_offset = 0x17C1F18;
    WinDll* const unity = rust.modules.GetModuleInfo("UnityPlayer.dll");
    auto gom = rust.Read<uint64_t>(unity->info.baseAddress + gom_offset);
    assert(gom);
    auto taggedObjects = rust.Read<uint64_t>(gom + 0x8);
    assert(taggedObjects);
    auto gameObject = rust.Read<uint64_t>(taggedObjects + 0x10);
    assert(gameObject);
    auto objClass = rust.Read<uint64_t>(gameObject + 0x30);
    assert(objClass);
    auto ent = rust.Read<uint64_t>(objClass + 0x18);
    assert(ent);
    return rust.Read<glm::mat4>(ent + 0xDC);
}

// reliable fallback
glm::mat4 getViewMatrix(glm::vec3 pos, float pitch, float yaw, float fov) {
    yaw = (-yaw) - 90.0f; // retarded fix?
    glm::vec3 front;
    front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    front.y = sin(glm::radians(pitch));
    front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    glm::vec3 cameraFront = glm::normalize(front);

    constexpr glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);

    // TODO: don't assume 1440p
    constexpr float width = 2560;
    constexpr float height = 1440;
    glm::mat4 project = glm::perspective(glm::radians(fov), width / height, 0.1f, 500.0f);
    glm::mat4 view = glm::lookAt(pos, pos + cameraFront, up);

    // correct vertical
    const glm::mat4 rotation = glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), glm::vec3(0.0f, 1.0f, 0.f));
    // flip on y axis
    constexpr glm::mat4 reflection = glm::mat4(
            -1.0, 0.0, 0.0, 0.0,
            0.0, 1.0, 0.0, 0.0,
            0.0, 0.0, 1.0, 0.0,
            0.0, 0.0, 0.0, 1.0);

    return project * ((rotation * reflection) * view);
}

glm::mat4 getViewMatrix0(WinProcess& rust) {
    static constexpr uint64_t gom_offset = 0x17C1F18; // TODO pattern finder
    static const uint64_t unity_player = rust.modules.GetModuleInfo("UnityPlayer.dll")->info.baseAddress;

    static uintptr_t camera = 0; // TODO: I think this changes when joining a new server
    if (!camera) {
        auto gom = rust.Read<uintptr_t>(unity_player + gom_offset);
        assert(gom);
        if (!gom) return {};
        auto tagged_objects = rust.Read<uintptr_t>(gom + 0x8);
        assert(tagged_objects);
        if (!tagged_objects) return {};
        auto game_object = rust.Read<uintptr_t>(tagged_objects + 0x10);
        assert(game_object);
        if (!game_object) return {};
        auto object_class = rust.Read<uintptr_t>(game_object + 0x30);
        assert(object_class);
        if (!object_class) return {};

        camera = rust.Read<uintptr_t>(object_class + 0x18);
    }

    if (camera) {
        return rust.Read<glm::mat4>(camera + 0xDC);
    } else {
        return {};
    }
}


// returns ndc (-1/1)
std::optional<glm::vec2> worldToScreen(const glm::mat4& matrix, const glm::vec3& worldPos) {
    glm::vec3 transVec{matrix[0][3], matrix[1][3], matrix[2][3]};
    glm::vec3 rightVec{matrix[0][0], matrix[1][0], matrix[2][0]};
    glm::vec3 upVec   {matrix[0][1], matrix[1][1], matrix[2][1]};

    float w = glm::dot(transVec, worldPos) + matrix[3][3];
    if ((w != w) || w < 0.098f) return std::nullopt;
    float y = glm::dot(upVec, worldPos) + matrix[3][1];
    float x = glm::dot(rightVec, worldPos) + matrix[3][0];

    // this is normally converted to screen space coords
    return glm::vec2(x / w, y / w);
}

constexpr const char* espVertexShader =
/*R"(
#version 330 core

layout(location = 0) in vec2 pos;

void main() {
    gl_Position.xyz = vec3(pos, 0.0);
    gl_Position.w = 1.0;
}
)";*/
R"(
#version 320 es

layout(std430, binding = 0) readonly buffer Boxes {
    vec2 vertices[][4];
};

void main() {
    vec2 pos = vertices[gl_InstanceID][gl_VertexID];
    gl_Position.xyz = vec3(pos, 0.0);
    gl_Position.w = 1.0;
}
)";

constexpr const char* espFragmentShader =
R"(
#version 320 es

out mediump vec3 color;

void main() {
  color = vec3(1.0, 0.0, 0.0);
}
)";


void renderEsp(const std::vector<std::array<glm::vec2, 4>>& boxes) {
    static GLuint programID = LoadShaders(espVertexShader, espFragmentShader);
    glUseProgram(programID);

    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ZERO);

    using type = std::decay_t<decltype(boxes)>::value_type;
    GLuint buffer; // The ID
    glGenBuffers(1, &buffer);
    //glEnableVertexAttribArray(0);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, buffer); // Set the buffer as the active array
    glBufferData(GL_SHADER_STORAGE_BUFFER, boxes.size() * sizeof(type), boxes.data(), GL_STATIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, buffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0); // unbind
    //glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr); // 2 floats per vertex
    glDrawArraysInstanced(GL_LINE_LOOP, 0, 4, boxes.size());

    //glDisableVertexAttribArray(0);
    glDeleteBuffers(1, &buffer);
}

constexpr const GLchar* textVertexShader = R"(
#version 330 core
layout (location = 0) in vec2 posIn;
layout (location = 1) in vec2 texCoordIn;
layout (location = 2) in vec3 textColorIn;

out vec2 TexCoord;
out vec3 TextColor;

uniform mat4 projection;

void main()
{
    vec4 pos = projection * vec4(posIn, 0.0, 1.0);
    gl_Position = vec4(pos.x, pos.y, 0.0, 1.0);
    TexCoord = texCoordIn;
    TextColor = textColorIn;
}
)";

constexpr const GLchar* textFragmentShader = R"(
#version 330 core

out vec4 FragColor;

in vec2 TexCoord;
in vec3 TextColor;

// texture samplers
uniform sampler2D texture1;

void main()
{
    float sampled = texture(texture1, TexCoord).r;
    FragColor = vec4(TextColor, sampled);
}
)";

void setTextUniforms(GLuint program) {
    glUniform1i(glGetUniformLocation(program, "texture1"), 0);
}

void setMatUniforms(GLuint program, const glm::mat4& proj) {
    glUniformMatrix4fv(glGetUniformLocation(program, "projection"), 1, GL_FALSE, &proj[0][0]);
}

void renderText(const Font& font, const std::vector<std::array<float, 7>>& text_vertices, const std::vector<std::array<GLuint, 3>>& text_indices) {
    static GLuint programID = LoadShaders(textVertexShader, textFragmentShader);
    flushErrors();
    glUseProgram(programID);

    GLuint vao, vbo, ebo;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ebo);

    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, (sizeof(float) * 7) * text_vertices.size(), text_vertices.data(), GL_DYNAMIC_DRAW);

    // position attribute
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 7 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);
    // texture coord attribute
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 7 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);
    // color attribute
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 7 * sizeof(float), (void*)(4 * sizeof(float)));
    glEnableVertexAttribArray(2);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(unsigned int) * 3 * text_indices.size(), text_indices.data(), GL_DYNAMIC_DRAW);

    // top left origin
    const auto proj = glm::ortho(0.0f, 2560.0f, 1440.0f, 0.0f, 0.1f, 100.0f);
    setMatUniforms(programID, proj);

    // render it yay
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glBindTexture(GL_TEXTURE_2D, font.texture);
    setTextUniforms(programID);

    glBindVertexArray(vao);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glDrawElements(GL_TRIANGLES, 3 * text_indices.size(), GL_UNSIGNED_INT, nullptr);

    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    glDeleteBuffers(1, &ebo);
}

struct EspString {
    float r, g, b;
    std::string str;
};
struct EspInfo {
    float x, y; // screen space coords
    std::vector<EspString> lines;
};

void renderEspText(const std::vector<EspInfo>& sections) {
    static Font font = initFont(15);

    std::vector<std::array<float, 7>> text_vertices; // x/y, textureX/textureY, r/g/b
    std::vector<std::array<GLuint, 3>> text_indices;

    unsigned int i = 0;
    for (const auto& section : sections) {
        float yOff = section.y + 15; // origin of text is the top left so start with is shifted down
        for (const auto& line : section.lines) {
            float xOff = section.x;
            const float left = xOff;
            float right = left;
            float bottom = yOff;
            std::vector<std::array<float, 7>> lineVertices;

            for (char cIn : line.str) {
                const char c = (cIn >= 32 && cIn < 128) ? cIn : '?';
                stbtt_aligned_quad q;
                stbtt_GetPackedQuad(font.cdata.data(), ATLAS_DIM, ATLAS_DIM, c - 32, &xOff, &yOff, &q, 0);
                right = q.x1;
                bottom = std::max(q.y1, bottom);

                const auto r = line.r, g = line.g, b = line.b;
                lineVertices.push_back({q.x0,q.y0, q.s0,q.t0,  r, g, b}); // top left
                lineVertices.push_back({q.x1,q.y0, q.s1,q.t0,  r, g, b}); // top right
                lineVertices.push_back({q.x1,q.y1, q.s1,q.t1,  r, g, b}); // bottom right
                lineVertices.push_back({q.x0,q.y1, q.s0,q.t1,  r, g, b}); // bottom left
            }
            // TODO: this is where centering logic needs to happen
            text_vertices.insert(text_vertices.end(), lineVertices.begin(), lineVertices.end());

            yOff += 15; // TODO: calculate this

            const unsigned int iEnd = i + line.str.length() * 4;
            for (; i < iEnd; i += 4) {
                text_indices.push_back({ i, i + 1, i + 2 }); // top left, top right, bottom right
                text_indices.push_back({ i, i + 2, i + 3 }); // top left, bottom right, bottom left
            }
        }
    }

    renderText(font, text_vertices, text_indices);
}

std::pair<glm::vec2, glm::vec2> getEspSides(glm::vec2 from, glm::vec2 target, float width) {
    glm::vec2 v = target - from;

    float dist = glm::sqrt(v.x * v.x + v.y * v.y);
    v = v / dist; // length of v is now 1
    v = v * (width / 2); // length of v is now width/2

    glm::vec2 leftV = glm::vec2(-v.y, v.x);
    glm::vec2 rightV = glm::vec2(v.y, -v.x);

    return {
      glm::vec2(target.x + leftV.x, target.y + leftV.y),
      glm::vec2(target.x + rightV.x, target.y + rightV.y)
    };
}

std::optional<std::array<glm::vec2, 4>> getEspBox(glm::vec3 myPos, glm::vec3 playerPos, glm::mat4 viewMatrix) {
    auto w2s = [&viewMatrix](glm::vec3 pos) {
        return worldToScreen(viewMatrix, pos);
    };

    auto [left, right] = getEspSides({myPos.x, myPos.z}, {playerPos.x, playerPos.z}, 1);

    // TODO: this can be optimized by w2s for 2 corners and then filling in the other 2
    auto topL = w2s({left.x, playerPos.y + 1.8, left.y});
    auto topR = w2s({right.x, playerPos.y + 1.8, right.y});
    auto bottomL = w2s({left.x, playerPos.y, left.y});
    auto bottomR = w2s({right.x, playerPos.y, right.y});

    if (!topL || !topR || !bottomL || !bottomR) return std::nullopt;

    //constexpr float width = 0.04;
    return {{
        *bottomL, // bottom left
        *bottomR, // bottom right
        *topR, // top right
        *topL, // top left
    }};
}


EspInfo getEspInfo(const std::array<glm::vec2, 4>& box, const player& local, const player& player) {
    const float w = 2560.0f;
    const float h = 1440.0f;

    const float ndcX = box[0].x;
    const float ndcY = box[0].y;
    const float x = ((w / 2.0f) * ndcX) + (w / 2.0f);
    const float y = (h / 2.0f) - ((h / 2.0f) * ndcY);

    EspString name = {
        .r = 1.0f, .g = 0.0f, .b = 0.0f,
        .str = player.name
    };
    EspString health = {
        .r = 1.0f, .g = 0.0f, .b = 0.0f,
        .str = fmt::format("{:.1f}hp", player.health)
    };
    auto dist = glm::distance(local.position, player.position);
    EspString distance = {
        .r = 1.0f, .g = 0.0f, .b = 0.0f,
        .str = fmt::format("{:.1f}m", dist)
    };
    //auto weapon = getHeldWeapon(player.handle);

    return {
        .x = x,
        .y = y,
        .lines = {
            name,
            health,
            distance
        }
    };
}

std::ostream& operator<<(std::ostream& out, const glm::mat4& matrix) {
    out << '[';
    for (int i = 0; i < 4; i++) {
        out << '[';
        for (int j = 0; j < 4; j++) {
            out << matrix[i][j] << ' ';
        }
        out << "]\n";
    }
    out << ']';

    return out;
}

std::pair<int, int> getWindowSize() {
    return {2560, 1440};
}

void renderOverlay(WinProcess& rust) {
    void test();

    /*std::vector<EspInfo> text = {
        {
            .x = 500,
            .y = 500,
            .lines = {
                {
                    .r = 1.0f,
                    .g = 0.0f,
                    .b = 0.0f,
                    .str = "OWO UWU OWO UWU"
                },
                {
                    .r = 0.0f,
                    .g = 1.0f,
                    .b = 0.0f,
                    .str = "Trans Rights"
                }
            }
        }
    };

    renderEspText(text);
    return;*/

    const auto localPtr = getLocalPlayer(rust);
    if (!localPtr) {
        return;
    }
    const std::vector players = getVisiblePlayers(rust);
    if (players.empty()) {
        return;
    }

    const player local = player{rust, localPtr};
    const glm::vec3 headPos = glm::vec3(local.position.x, local.position.y + 1.6f, local.position.z); // TODO: get head bone position
    // ConVar_Graphics_StaticFields::_fov
    //glm::mat4 viewMatrix = getViewMatrix(headPos, local.angles.x, local.angles.y, 90.0f);
    glm::mat4 viewMatrix = getViewMatrix0(rust);

    std::vector<std::array<glm::vec2, 4>> espBoxes;
    std::vector<EspInfo> espText;
    for (const auto& player : players) {
        // TODO: filter local player
        auto pos = player.position;
        std::optional bpx = getEspBox(headPos, pos, viewMatrix);
        if (bpx) {
            espText.push_back(getEspInfo(*bpx, local, player));
            espBoxes.push_back(*bpx);
        }
    }

    renderEsp(espBoxes);
    renderEspText(espText);

    /*renderEsp({
            {glm::vec2{0.0f, 0.0f}, glm::vec2{0.0f, 0.2f}, glm::vec2{0.2f, 0.2f}, glm::vec2{0.2f, 0.0f}},
            {glm::vec2{-0.5f, -0.5f}, glm::vec2{-0.5f, -0.3f}, glm::vec2{-0.3f, -0.3f}, glm::vec2{-0.3f, -0.5f}}
    });*/
}


void checkShaderError(GLuint id) {
    int infoLogLength = 0;
    glGetShaderiv(id, GL_INFO_LOG_LENGTH, &infoLogLength);
    if (infoLogLength) {
        std::string errorMessage(infoLogLength, '\0');
        glGetShaderInfoLog(id, infoLogLength, nullptr, errorMessage.data());
        std::cout << errorMessage << std::endl;
        exit(1);
    }
}

void checkProgramError(GLuint id) {
    int infoLogLength = 0;
    glGetProgramiv(id, GL_INFO_LOG_LENGTH, &infoLogLength);
    if (infoLogLength) {
        std::string errorMessage(infoLogLength, '\0');
        glGetProgramInfoLog(id, infoLogLength, nullptr, errorMessage.data());
        std::cout << errorMessage << std::endl;
        exit(1);
    }
}

GLuint LoadShaders(const char* vertexShaderCode, const char* fragmentShaderCode) {

    // Create the shaders
    GLuint VertexShaderID = glCreateShader(GL_VERTEX_SHADER);
    GLuint FragmentShaderID = glCreateShader(GL_FRAGMENT_SHADER);

    GLint Result = GL_FALSE;

    // Compile Vertex Shader
    puts("Compiling vertex shader");
    glShaderSource(VertexShaderID, 1, &vertexShaderCode , nullptr);
    glCompileShader(VertexShaderID);

    // Check Vertex Shader
    glGetShaderiv(VertexShaderID, GL_COMPILE_STATUS, &Result);
    if (!Result) {
        std::cout << "vertex shader error" << std::endl;
        checkShaderError(VertexShaderID);
    }

    // Compile Fragment Shader
    puts("Compiling fragment shader");
    glShaderSource(FragmentShaderID, 1, &fragmentShaderCode , nullptr);
    glCompileShader(FragmentShaderID);

    // Check Fragment Shader
    glGetShaderiv(FragmentShaderID, GL_COMPILE_STATUS, &Result);
    if (!Result) {
        std::cout << "fragment shader error" << std::endl;
        checkShaderError(FragmentShaderID);
    }


    // Link the program
    printf("Linking program\n");
    GLuint ProgramID = glCreateProgram();
    glAttachShader(ProgramID, VertexShaderID);
    glAttachShader(ProgramID, FragmentShaderID);
    glLinkProgram(ProgramID);

    // Check the program
    glGetProgramiv(ProgramID, GL_LINK_STATUS, &Result);
    if (!Result) {
        std::cout << "link error?" << std::endl;
        checkProgramError(ProgramID);
    }

    glDetachShader(ProgramID, VertexShaderID);
    glDetachShader(ProgramID, FragmentShaderID);

    glDeleteShader(VertexShaderID);
    glDeleteShader(FragmentShaderID);

    return ProgramID;
}

void test() {
    constexpr const char* testVertexShader =
            R"(
#version 330 core

layout(location = 0) in vec2 vertexPos;

void main() {
    gl_Position.xyz = vec3(vertexPos.x, vertexPos.y, 0.0);
    gl_Position.w = 1.0;
}
)";

    constexpr const char* testFragmentShader =
            R"(
#version 330 core

out vec3 color;

void main() {
  color = vec3(1,0,0);
}
)";

    static GLuint programID = LoadShaders(testVertexShader, testFragmentShader);
    glUseProgram(programID);

    float line[] = {
            0.0, 0.0,
            0.5, 0.5//(float)w, (float)h
    };

    GLuint buffer; // The ID, kind of a pointer for VRAM
    glGenBuffers(1, &buffer); // Allocate memory for the triangle

    glBindBuffer(GL_ARRAY_BUFFER, buffer); // Set the buffer as the active array
    glBufferData(GL_ARRAY_BUFFER, sizeof(line), line, GL_STATIC_DRAW); // Fill the buffer with data
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr); // Specify how the buffer is converted to vertices
    glEnableVertexAttribArray(0); // Enable the vertex array
    glDrawArrays(GL_LINES, 0, 2);
    glDisableVertexAttribArray(0);
    glDeleteBuffers(1, &buffer);
}