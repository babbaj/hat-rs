#include <iostream>
#include <vector>

#include "overlay.h"

#define GL_GLEXT_PROTOTYPES
#include <SDL2/SDL_opengl.h>
#include <SDL2/SDL.h>

#include <glm/vec3.hpp>

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

constexpr const char* vertexShader =
R"(
#version 330 core

layout(location = 0) in vec2 vertexPos;

void main() {
    gl_Position.xyz = vec3(vertexPos.x, vertexPos.y, 0.0);
    gl_Position.w = 1.0;
}
)";

constexpr const char* fragmentShader =
R"(
#version 330 core

out vec3 color;

void main(){
  color = vec3(1,0,0);
}
)";

void renderOverlay(EGLDisplay dpy, EGLSurface surface) {
    if (lg_window == nullptr) {
        std::cerr << "Failed to set window\n";
        std::terminate();
    }

    int width, height;
    SDL_GetWindowSize(lg_window, &width, &height);

    static GLuint programID = LoadShaders( vertexShader, fragmentShader);
    glUseProgram(programID);

    float line[] = {
            0.0, 0.0,
            0.5, 0.5//(float)w, (float)h
    };

    GLuint buffer; // The ID, kind of a pointer for VRAM
    glGenBuffers(1, &buffer); // Allocate memory for the triangle
    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, buffer); // Set the buffer as the active array
    glBufferData(GL_ARRAY_BUFFER, sizeof(line), line, GL_STATIC_DRAW); // Fill the buffer with data
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr); // Specify how the buffer is converted to vertices
    glEnableVertexAttribArray(0); // Enable the vertex array
    glDrawArrays(GL_LINES, 0, 2);
    glDisableVertexAttribArray(0);
    glDeleteBuffers(1, &buffer);
    //puts("overlay");
}

void checkError(GLuint id) {
    int infoLogLength;
    glGetShaderiv(id, GL_INFO_LOG_LENGTH, &infoLogLength);
    if (infoLogLength) {
        std::string errorMessage(infoLogLength + 1, '\0');
        glGetShaderInfoLog(id, infoLogLength, nullptr, errorMessage.data());
        std::cout << errorMessage << '\n';
    }
}

GLuint LoadShaders(const char* vertexShaderCode, const char* fragmentShaderCode) {

    // Create the shaders
    GLuint VertexShaderID = glCreateShader(GL_VERTEX_SHADER);
    GLuint FragmentShaderID = glCreateShader(GL_FRAGMENT_SHADER);

    GLint Result = GL_FALSE;
    int InfoLogLength;

    // Compile Vertex Shader
    puts("Compiling vertex shader");
    glShaderSource(VertexShaderID, 1, &vertexShaderCode , nullptr);
    glCompileShader(VertexShaderID);

    // Check Vertex Shader
    glGetShaderiv(VertexShaderID, GL_COMPILE_STATUS, &Result);
    glGetShaderiv(VertexShaderID, GL_INFO_LOG_LENGTH, &InfoLogLength);
    checkError(VertexShaderID);

    // Compile Fragment Shader
    puts("Compiling fragment shader");
    glShaderSource(FragmentShaderID, 1, &fragmentShaderCode , nullptr);
    glCompileShader(FragmentShaderID);

    // Check Fragment Shader
    glGetShaderiv(FragmentShaderID, GL_COMPILE_STATUS, &Result);
    glGetShaderiv(FragmentShaderID, GL_INFO_LOG_LENGTH, &InfoLogLength);
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