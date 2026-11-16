#pragma once
#include <GL/glew.h>
#include <string>


class Shader {
public:
    Shader(const std::string& vertPath, const std::string& fragPath);
    ~Shader();
    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;

    void use() const { glUseProgram(id_); }
    void setInt(const char* name, int v) const;
    void setFloat(const char* name, float v) const;
    void setVec2(const char* name, float x, float y) const;
    void setFloats(const char* name, const float* v, int count) const;

private:
    GLint loc(const char* name) const { return glGetUniformLocation(id_, name); }
    GLuint id_ = 0;
};
