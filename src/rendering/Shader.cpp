#include "rendering/Shader.h"

#include <glm/gtc/type_ptr.hpp>

#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>

Shader::~Shader()
{
    if (mProgram != 0)
    {
        glDeleteProgram(mProgram);
    }
}

std::string Shader::readTextFile(const std::string& path)
{
    std::ifstream input(path);
    if (!input)
    {
        std::cerr << "Could not open shader file: " << path << '\n';
        return {};
    }

    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

bool Shader::compileShader(GLuint shaderId, const std::string& source, const std::string& debugName)
{
    const char* sourcePtr = source.c_str();
    glShaderSource(shaderId, 1, &sourcePtr, nullptr);
    glCompileShader(shaderId);

    GLint compileStatus = GL_FALSE;
    glGetShaderiv(shaderId, GL_COMPILE_STATUS, &compileStatus);
    if (compileStatus == GL_TRUE)
    {
        return true;
    }

    GLint logLength = 0;
    glGetShaderiv(shaderId, GL_INFO_LOG_LENGTH, &logLength);
    std::vector<GLchar> log(static_cast<size_t>(logLength), '\0');
    if (logLength > 0)
    {
        glGetShaderInfoLog(shaderId, logLength, nullptr, log.data());
    }

    std::cerr << "Shader compilation failed (" << debugName << "):\n" << log.data() << '\n';
    return false;
}

bool Shader::loadFromFiles(const std::string& vertexPath, const std::string& fragmentPath)
{
    const std::string vertexSource = readTextFile(vertexPath);
    const std::string fragmentSource = readTextFile(fragmentPath);
    if (vertexSource.empty() || fragmentSource.empty())
    {
        return false;
    }

    const GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    const GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);

    if (!compileShader(vertexShader, vertexSource, vertexPath) ||
        !compileShader(fragmentShader, fragmentSource, fragmentPath))
    {
        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);
        return false;
    }

    const GLuint program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    GLint linkStatus = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &linkStatus);
    if (linkStatus != GL_TRUE)
    {
        GLint logLength = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logLength);
        std::vector<GLchar> log(static_cast<size_t>(logLength), '\0');
        if (logLength > 0)
        {
            glGetProgramInfoLog(program, logLength, nullptr, log.data());
        }

        std::cerr << "Program link failed:\n" << log.data() << '\n';
        glDeleteProgram(program);
        return false;
    }

    if (mProgram != 0)
    {
        glDeleteProgram(mProgram);
    }

    mProgram = program;
    return true;
}

void Shader::use() const
{
    glUseProgram(mProgram);
}

void Shader::setInt(const std::string& uniformName, int value) const
{
    const GLint location = glGetUniformLocation(mProgram, uniformName.c_str());
    if (location >= 0)
    {
        glUniform1i(location, value);
    }
}

void Shader::setFloat(const std::string& uniformName, float value) const
{
    const GLint location = glGetUniformLocation(mProgram, uniformName.c_str());
    if (location >= 0)
    {
        glUniform1f(location, value);
    }
}

void Shader::setVec2(const std::string& uniformName, const glm::vec2& value) const
{
    const GLint location = glGetUniformLocation(mProgram, uniformName.c_str());
    if (location >= 0)
    {
        glUniform2f(location, value.x, value.y);
    }
}

void Shader::setVec3(const std::string& uniformName, const glm::vec3& value) const
{
    const GLint location = glGetUniformLocation(mProgram, uniformName.c_str());
    if (location >= 0)
    {
        glUniform3f(location, value.x, value.y, value.z);
    }
}

void Shader::setMat4(const std::string& uniformName, const glm::mat4& matrix) const
{
    const GLint location = glGetUniformLocation(mProgram, uniformName.c_str());
    if (location >= 0)
    {
        glUniformMatrix4fv(location, 1, GL_FALSE, glm::value_ptr(matrix));
    }
}

void Shader::setVec2Array(const std::string& uniformName, const glm::vec2* data, int count) const
{
    const GLint location = glGetUniformLocation(mProgram, uniformName.c_str());
    if (location >= 0)
    {
        glUniform2fv(location, count, reinterpret_cast<const GLfloat*>(data));
    }
}

void Shader::setVec3Array(const std::string& uniformName, const glm::vec3* data, int count) const
{
    const GLint location = glGetUniformLocation(mProgram, uniformName.c_str());
    if (location >= 0)
    {
        glUniform3fv(location, count, reinterpret_cast<const GLfloat*>(data));
    }
}

void Shader::setFloatArray(const std::string& uniformName, const float* data, int count) const
{
    const GLint location = glGetUniformLocation(mProgram, uniformName.c_str());
    if (location >= 0)
    {
        glUniform1fv(location, count, data);
    }
}
