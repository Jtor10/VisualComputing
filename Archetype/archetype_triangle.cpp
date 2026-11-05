// archetype_triangle.cpp
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <GL/glew.h>

#include "vc.hpp"

static std::string shader(const std::string& name) {
    // Busca junto al ejecutable; si no, en ../../shaders
    std::ifstream f(name);
    if (f) return vc::readFile(name);
    return vc::readFile("../../shaders/" + name);
}

int main() {
    if (!glfwInit()) { std::cerr << "glfwInit falló\n"; return 1; }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(600, 400, "archetype_triangle",
                                          nullptr, nullptr);
    if (!window) { std::cerr << "No se pudo crear la ventana\n"; glfwTerminate(); return 1; }
    glfwMakeContextCurrent(window);

    glewExperimental = GL_TRUE;
    GLenum err = glewInit();
    if (err != GLEW_OK) {
        std::cerr << "glewInit falló: " << glewGetErrorString(err) << "\n";
        return 1;
    }
    // GLEW a veces deja un GL_INVALID_ENUM en core profile
    while (glGetError() != GL_NO_ERROR) {}

    auto prog = vc::createProgramInfo(shader("archetype_triangle.vert"),
                                      shader("archetype_triangle.frag"));

    auto triangle = vc::createBufferInfo(prog, {
        {"aPosition",    3, {-86.6f, -50, 0,   86.6f, -50, 0,   0, 100, 0}},
        {"aVertexColor", 4, {1, 0, 0, 1,   0, 1, 0, 1,   0, 0, 1, 1}},
    });

    auto proj = vc::perspective(3.14159265f / 3, 600.0f / 400, 0.1f, 1000);
    auto view = vc::translation(0, 0, -400);

    while (!glfwWindowShouldClose(window)) {
        int w, h;
        glfwGetFramebufferSize(window, &w, &h);
        glViewport(0, 0, w, h);
        glClearColor(20 / 255.0f, 20 / 255.0f, 20 / 255.0f, 1);
        glClear(GL_COLOR_BUFFER_BIT);

        auto mv = vc::multiply(view,
                               vc::rotationY(static_cast<float>(glfwGetTime())));
        glUseProgram(prog.program);
        vc::setUniform(prog, "uProjectionMatrix", proj);
        vc::setUniform(prog, "uModelViewMatrix", mv);
        vc::drawBufferInfo(triangle);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }
    glfwTerminate();
}
