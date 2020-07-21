#include <iostream>

#include "overlay.h"

#define GL_GLEXT_PROTOTYPES
#include <SDL2/SDL_opengl.h>

#include <SDL2/SDL.h>
#include <dlfcn.h>

static SDL_Window* lg_window;


extern "C" DECLSPEC SDL_Window *SDLCALL SDL_CreateWindow(const char *title,int x, int y, int w, int h, Uint32 flags) {
    static auto *createwindow = (decltype(&SDL_CreateWindow)) dlsym(RTLD_NEXT, "SDL_CreateWindow");
    puts("hooked SDL_CreateWindow");
    auto* out = createwindow(title, x, y, w, h, flags);
    lg_window = out;
    return out;
}

void renderOverlay(EGLDisplay dpy, EGLSurface surface) {
    if (lg_window == nullptr) {
        std::cerr << "Failed to set window\n";
        std::terminate();
    }

    int w, h;
    SDL_GetWindowSize(lg_window, &w, &h);

    //auto error = glGetError();
    //std::cout << "error = " << error << '\n';

    float line[] = {
            0.0, 0.0,
            500.0, 500.0//(float)w, (float)h
    };

    unsigned int buffer; // The ID, kind of a pointer for VRAM
    glGenBuffers(1, &buffer); // Allocate memory for the triangle
    glBindBuffer(GL_ARRAY_BUFFER, buffer); // Set the buffer as the active array
    glBufferData(GL_ARRAY_BUFFER, 4 * sizeof(float), line, GL_STATIC_DRAW); // Fill the buffer with data
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr); // Specify how the buffer is converted to vertices
    glEnableVertexAttribArray(0); // Enable the vertex array
    glDrawArrays(GL_LINES, 0, 2);
}