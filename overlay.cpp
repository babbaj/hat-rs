#include <iostream>
#include <vector>
#include <optional>
#include <iterator>

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
#include <fstream>

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

glm::mat4x4 getViewMatrix(WinProcess& rust) {
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
    return rust.Read<glm::mat4x4>(ent + 0xDC);
}

// returns ndc (-1/1)
std::optional<glm::vec2> worldToScreen(const glm::mat4x4& viewMatrix, const glm::vec3& worldPos) {
    glm::vec3 transVec = glm::vec3(viewMatrix[0][3], viewMatrix[1][3], viewMatrix[2][3]);
    glm::vec3 rightVec = glm::vec3(viewMatrix[0][0], viewMatrix[1][0], viewMatrix[2][0]);
    glm::vec3 upVec = glm::vec3(viewMatrix[0][1], viewMatrix[1][1], viewMatrix[2][1]);

    float w = glm::dot(transVec, worldPos) + viewMatrix[3][3];
    if (w < 0.098f) return std::nullopt;
    float y = glm::dot(upVec, worldPos) + viewMatrix[3][1];
    float x = glm::dot(rightVec, worldPos) + viewMatrix[3][0];

    // this is normally converted to screen space coords
    return glm::vec2(x / w, y / w);
}

constexpr const char* espVertexShader =
R"(
#version 330 core

layout(location = 0) in vec2 pos;

void main() {
    gl_Position.xyz = vec3(pos, 0.0);
    gl_Position.w = 1.0;
}
)";

constexpr const char* espFragmentShader =
        R"(
#version 330 core

out vec3 color;

void main() {
  color = vec3(1,0,0);
}
)";

glm::vec3 toGlm(const vector3& vec) {
    return glm::vec3(vec.x, vec.y, vec.z);
}

std::ostream& operator<<(std::ostream& out, const glm::mat4x4 matrix) {
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

void renderEsp(std::vector<glm::vec2> points) {
    static GLuint programID = LoadShaders(espVertexShader, espFragmentShader);
    glUseProgram(programID);

    GLuint buffer; // The ID
    glGenBuffers(1, &buffer);
    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, buffer); // Set the buffer as the active array
    glBufferData(GL_ARRAY_BUFFER, points.size() * sizeof(glm::vec2), points.data(), GL_STATIC_DRAW); // Fill the buffer with data
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(glm::vec2), nullptr); // Specify how the buffer is converted to vertices
    glEnableVertexAttribArray(0); // Enable the vertex array
    glDrawArrays(GL_POINTS, 0, points.size());
    glDisableVertexAttribArray(0);
    glDeleteBuffers(1, &buffer);
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

    std::vector players = getVisiblePlayers(rust);
    std::vector<glm::vec2> screenPositions; screenPositions.reserve(players.size());

    glm::mat4x4 viewMatrix = getViewMatrix(rust);
    //std::cout << "matrix = " << viewMatrix << '\n';
    for (const auto& player : players) {
        // TODO: filter local player
        auto pos = toGlm(player.position);
        std::optional screenPos = worldToScreen(viewMatrix, pos);
        if (screenPos) {
            screenPositions.push_back(*screenPos);
        }
    }

    //std::cout << players.size() << " players\n";
    //std::cout << screenPositions.size() << " visible players\n";
    renderEsp(screenPositions);
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
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr); // Specify how the buffer is converted to vertices
    glEnableVertexAttribArray(0); // Enable the vertex array
    glDrawArrays(GL_POINTS, 0, 2);
    glDisableVertexAttribArray(0);
    glDeleteBuffers(1, &buffer);
}