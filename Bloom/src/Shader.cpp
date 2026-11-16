#include "Shader.hpp"

#include <fstream>
#include <sstream>
#include <stdexcept>

static std::string readFile(const std::string& path)
{
    std::ifstream f(path);
    if (!f) throw std::runtime_error("No se pudo abrir el shader: " + path);
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

static GLuint compile(GLenum type, const std::string& path)
{
    std::string src = readFile(path);
    const char* c = src.c_str();
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &c, nullptr);
    glCompileShader(s);

    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[4096];
        glGetShaderInfoLog(s, sizeof(log), nullptr, log);
        glDeleteShader(s);
        throw std::runtime_error("Error compilando " + path + ":\n" + log);
    }
    return s;
}

Shader::Shader(const std::string& vertPath, const std::string& fragPath)
{
    GLuint vs = compile(GL_VERTEX_SHADER, vertPath);
    GLuint fs = compile(GL_FRAGMENT_SHADER, fragPath);

    id_ = glCreateProgram();
    glAttachShader(id_, vs);
    glAttachShader(id_, fs);
    glLinkProgram(id_);
    glDeleteShader(vs);
    glDeleteShader(fs);

    GLint ok = 0;
    glGetProgramiv(id_, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[4096];
        glGetProgramInfoLog(id_, sizeof(log), nullptr, log);
        glDeleteProgram(id_);
        throw std::runtime_error("Error enlazando " + vertPath + " + " + fragPath + ":\n" + log);
    }
}

Shader::~Shader() { if (id_) glDeleteProgram(id_); }

void Shader::setInt(const char* n, int v) const            { glUniform1i(loc(n), v); }
void Shader::setFloat(const char* n, float v) const        { glUniform1f(loc(n), v); }
void Shader::setVec2(const char* n, float x, float y) const { glUniform2f(loc(n), x, y); }
void Shader::setFloats(const char* n, const float* v, int c) const { glUniform1fv(loc(n), c, v); }
