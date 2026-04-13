#pragma once

#include <GL/glew.h>
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include <string>

class Shader
{
public:
    Shader() = default;
    ~Shader();

    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;

    bool loadFromFiles(const std::string& vertexPath, const std::string& fragmentPath);
    void use() const;

    void setInt(const std::string& uniformName, int value) const;
    void setFloat(const std::string& uniformName, float value) const;
    void setVec2(const std::string& uniformName, const glm::vec2& value) const;
    void setVec3(const std::string& uniformName, const glm::vec3& value) const;
    void setMat4(const std::string& uniformName, const glm::mat4& matrix) const;
    void setVec2Array(const std::string& uniformName, const glm::vec2* data, int count) const;
    void setVec3Array(const std::string& uniformName, const glm::vec3* data, int count) const;
    void setFloatArray(const std::string& uniformName, const float* data, int count) const;

private:
    static std::string readTextFile(const std::string& path);
    static bool compileShader(GLuint shaderId, const std::string& source, const std::string& debugName);

    GLuint mProgram = 0;
};
