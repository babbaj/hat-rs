#pragma once

#define GL_GLEXT_PROTOTYPES
#include <SDL2/SDL_opengl.h>
#include <SDL2/SDL.h>

GLuint LoadShaders(const char* vertexShaderCode, const char* fragmentShaderCode, const char* geometryShaderCode = nullptr);