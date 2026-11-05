#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <GL/glew.h>

#include "vc.hpp"
#include <iostream>
#include <algorithm>

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>   // o "stb_image.h" si lo copiaste local

// ── Carga de imagen (HDR o LDR) ─────────────────────────────────────────
GLuint loadTexture(const char* path, bool* isHDR) {
    int w, h, channels;
    stbi_set_flip_vertically_on_load(true);
    // Primero intenta HDR (float)
    float* fdata = stbi_loadf(path, &w, &h, &channels, 3);
    if (fdata) {
        *isHDR = true;
        GLuint tex;
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, w, h, 0,
                     GL_RGB, GL_FLOAT, fdata);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        stbi_image_free(fdata);
        std::cout << "HDR: " << path << " (" << w << "x" << h << ")\n";
        return tex;
    }

    // Si falla, intenta LDR
    stbi_set_flip_vertically_on_load(true);
    unsigned char* bdata = stbi_load(path, &w, &h, &channels, 3);
    if (!bdata) {
        std::cerr << "No se pudo cargar: " << path << "\n";
        std::exit(1);
    }
    *isHDR = false;
    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, w, h, 0,
                 GL_RGB, GL_UNSIGNED_BYTE, bdata);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    stbi_image_free(bdata);
    std::cout << "LDR: " << path << " (" << w << "x" << h << ")\n";
    return tex;
}

// ── Estado ──────────────────────────────────────────────────────────────
struct Params {
    float exposure    = 0.0f;
    float op          = 2.0f;
    float contrast    = 1.0f;
    float temperature = 0.0f;
    int   mode        = 0;      // 0 escena, 1 plot
    int   useTexture  = 0;      // 0 sintética, 1 imagen
    bool  hasTexture  = false;  // ¿se cargó una imagen?
};

static void keyCallback(GLFWwindow* w, int key, int, int action, int) {
    if (action != GLFW_PRESS && action != GLFW_REPEAT) return;
    Params* p = (Params*)glfwGetWindowUserPointer(w);
    switch (key) {
        case GLFW_KEY_ESCAPE: glfwSetWindowShouldClose(w, GLFW_TRUE); break;

        case GLFW_KEY_UP:    p->exposure = std::min(p->exposure + 0.25f,  4.0f); break;
        case GLFW_KEY_DOWN:  p->exposure = std::max(p->exposure - 0.25f, -4.0f); break;

        case GLFW_KEY_1: p->op = 0.0f; break;
        case GLFW_KEY_2: p->op = 1.0f; break;
        case GLFW_KEY_3: p->op = 2.0f; break;

        case GLFW_KEY_RIGHT: p->contrast = std::min(p->contrast + 0.05f, 2.0f); break;
        case GLFW_KEY_LEFT:  p->contrast = std::max(p->contrast - 0.05f, 0.5f); break;

        case GLFW_KEY_W: p->temperature = std::min(p->temperature + 0.03f,  0.3f); break;
        case GLFW_KEY_S: p->temperature = std::max(p->temperature - 0.03f, -0.3f); break;

        case GLFW_KEY_TAB: p->mode = 1 - p->mode; break;

        // ── Toggle sintética - imagen ───────────────────────────────────
        case GLFW_KEY_T:
            if (p->hasTexture) p->useTexture = 1 - p->useTexture;
            break;

        case GLFW_KEY_R: *p = Params{ .hasTexture = p->hasTexture }; break;
    }
}

int main(int argc, char** argv) {
    const char* imgPath = (argc > 1) ? argv[1] : nullptr;

    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(600, 400, "tone", nullptr, nullptr);
    glfwMakeContextCurrent(window);

    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) { std::cerr << "glewInit falló\n"; return 1; }
    while (glGetError() != GL_NO_ERROR) {}

    Params params;
    glfwSetWindowUserPointer(window, &params);
    glfwSetKeyCallback(window, keyCallback);

    // ── Cargar textura  ────────────────────────────
    GLuint sceneTex = 0;
    if (imgPath) {
        bool isHDR = false;
        sceneTex = loadTexture(imgPath, &isHDR);
        params.hasTexture = true;
        params.useTexture = 1;   // arrancar mostrando la imagen
        std::cout << (isHDR ? "Modo HDR real.\n"
                            : "Modo LDR\n");
    } else {
        std::cout << "Sin imagen. Sintética por defecto. "
                     "Pasa una ruta como argumento para cargar una imagen.\n";
    }

    // ── Programa ────────────────────────────────────────────────────────
    auto prog = vc::createProgramInfo(
        vc::readFile("tone.vert"),
        vc::readFile("tone.frag"));

    // ── Fullscreen quad ────────────────────────────────────────────────
    auto quad = vc::createBufferInfo(prog, {
        {"aPosition", 2, {
            -1, -1,   1, -1,   -1, 1,
            -1,  1,   1, -1,    1, 1,
        }},
    });

    GLint uExposure    = glGetUniformLocation(prog.program, "uExposure");
    GLint uOperator    = glGetUniformLocation(prog.program, "uOperator");
    GLint uContrast    = glGetUniformLocation(prog.program, "uContrast");
    GLint uTemperature = glGetUniformLocation(prog.program, "uTemperature");
    GLint uMode        = glGetUniformLocation(prog.program, "uMode");
    GLint uUseTexture  = glGetUniformLocation(prog.program, "uUseTexture");
    GLint uScene       = glGetUniformLocation(prog.program, "uScene");

    while (!glfwWindowShouldClose(window)) {
        int w, h;
        glfwGetFramebufferSize(window, &w, &h);
        glViewport(0, 0, w, h);
        glClear(GL_COLOR_BUFFER_BIT);

        glUseProgram(prog.program);
        glUniform1f(uExposure,    params.exposure);
        glUniform1f(uOperator,    params.op);
        glUniform1f(uContrast,    params.contrast);
        glUniform1f(uTemperature, params.temperature);
        glUniform1i(uMode,        params.mode);
        glUniform1i(uUseTexture,  params.useTexture);
        glUniform1i(uScene,       0);  // textura en unidad 0

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, sceneTex);

        vc::drawBufferInfo(quad);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }
    glfwTerminate();
}
