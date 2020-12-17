#include <iostream>
#include <vector>
#include <optional>
#include <array>

#include "utils.h"
#include "overlay.h"

#define GL_GLEXT_PROTOTYPES
#include <SDL2/SDL_opengl.h>
#include <SDL2/SDL.h>

#include <glm/vec3.hpp>
#include <glm/vec2.hpp>
#include <glm/mat4x4.hpp>
#include <glm/ext.hpp>

#include <dlfcn.h>

static SDL_Window* lg_window;


extern "C" DECLSPEC SDL_Window *SDLCALL SDL_CreateWindow(const char *title,int x, int y, int w, int h, Uint32 flags) {
    static auto *createwindow = (decltype(&SDL_CreateWindow)) dlsym(RTLD_NEXT, "SDL_CreateWindow");
    puts("hooked SDL_CreateWindow");
    auto* out = createwindow(title, x, y, w, h, flags);
    lg_window = out;
    return out;
}

void printError() {
    auto error = glGetError();
    std::cout << "error = " << error << '\n';
}

GLuint LoadShaders(const char* vertexShaderCode, const char* fragmentShaderCode);

[[deprecated]] glm::mat4 getViewMatrix(WinProcess& rust) {
    constexpr auto gom_offset = 0x17A6AD8;
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

// returns ndc (-1/1)
std::optional<glm::vec2> worldToScreen(const glm::mat4& matrix, const glm::vec3& worldPos) {
    glm::vec3 transVec{matrix[0][3], matrix[1][3], matrix[2][3]};
    glm::vec3 rightVec{matrix[0][0], matrix[1][0], matrix[2][0]};
    glm::vec3 upVec   {matrix[0][1], matrix[1][1], matrix[2][1]};

    float w = glm::dot(transVec, worldPos) + matrix[3][3];
    if (w < 0.098f) return std::nullopt;
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

glm::vec3 toGlm(const vector3& vec) {
    return glm::vec3(vec.x, vec.y, vec.z);
}


void renderEsp(const std::vector<std::array<glm::vec2, 4>>& boxes) {
    static GLuint programID = LoadShaders(espVertexShader, espFragmentShader);
    glUseProgram(programID);

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

void renderOverlay(EGLDisplay dpy, EGLSurface surface, WinProcess& rust) {
    if (lg_window == nullptr) {
        std::cerr << "Failed to set window\n";
        std::terminate();
    }

    int width, height;
    SDL_GetWindowSize(lg_window, &width, &height);

    void test();
    //test();
    //return;

    const auto localPtr = getLocalPlayer(rust);
    if (!localPtr) {
        return;
    }
    const std::vector players = getVisiblePlayers(rust);
    if (players.empty()) {
        return;
    }
    std::vector<std::array<glm::vec2, 4>> espBoxes;
    //glm::mat4 viewMatrix = getViewMatrix(rust);
    const player local = player{rust, localPtr};
    const glm::vec3 headPos = glm::vec3(local.position.x, local.position.y + 1.6f, local.position.z); // TODO: get head bone posiiton
    // ConVar_Graphics_StaticFields::_fov
    glm::mat4 viewMatrix = getViewMatrix(headPos, local.angles.x, local.angles.y, 90.0f);

    //std::cout << viewMatrix << '\n';
    for (const auto& player : players) {
        // TODO: filter local player
        auto pos = toGlm(player.position);
        std::optional bpx = getEspBox(headPos, pos, viewMatrix);
        if (bpx) {
            espBoxes.push_back(*bpx);
        }
    }

    //std::cout << espBoxes.size() << " visible players\n";
    renderEsp(espBoxes);

    /*renderEsp({
            {glm::vec2{0.0f, 0.0f}, glm::vec2{0.0f, 0.2f}, glm::vec2{0.2f, 0.2f}, glm::vec2{0.2f, 0.0f}},
            {glm::vec2{-0.5f, -0.5f}, glm::vec2{-0.5f, -0.3f}, glm::vec2{-0.3f, -0.3f}, glm::vec2{-0.3f, -0.5f}}
    });*/
}


void checkError(GLuint id) {
    int infoLogLength = 0;
    glGetShaderiv(id, GL_INFO_LOG_LENGTH, &infoLogLength);
    if (infoLogLength) {
        std::string errorMessage(infoLogLength, '\0');
        glGetShaderInfoLog(id, infoLogLength, nullptr, errorMessage.data());
        std::cout << errorMessage << '\n';
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
    checkError(VertexShaderID);

    // Compile Fragment Shader
    puts("Compiling fragment shader");
    glShaderSource(FragmentShaderID, 1, &fragmentShaderCode , nullptr);
    glCompileShader(FragmentShaderID);

    // Check Fragment Shader
    glGetShaderiv(FragmentShaderID, GL_COMPILE_STATUS, &Result);
    checkError(FragmentShaderID);

    // Link the program
    printf("Linking program\n");
    GLuint ProgramID = glCreateProgram();
    glAttachShader(ProgramID, VertexShaderID);
    glAttachShader(ProgramID, FragmentShaderID);
    glLinkProgram(ProgramID);

    // Check the program
    glGetProgramiv(ProgramID, GL_LINK_STATUS, &Result);
    checkError(ProgramID);

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


    int width, height;
    SDL_GetWindowSize(lg_window, &width, &height);

    GLuint buffer; // The ID, kind of a pointer for VRAM
    glGenBuffers(1, &buffer); // Allocate memory for the triangle
    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, buffer); // Set the buffer as the active array
    glBufferData(GL_ARRAY_BUFFER, sizeof(line), line, GL_STATIC_DRAW); // Fill the buffer with data
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr); // Specify how the buffer is converted to vertices
    glEnableVertexAttribArray(0); // Enable the vertex array
    glDrawArrays(GL_LINES, 0, 2);
    glDisableVertexAttribArray(0);
    glDeleteBuffers(1, &buffer);
}