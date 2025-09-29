#pragma once
#include <GL/glew.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <unordered_map>

class Shader
{
public:
    // Shader program ID
    unsigned int ID;

    // Constructor generates the shader on the fly
    Shader(const char* vertexPath, const char* fragmentPath, const char* geometryPath = nullptr);

    // Constructor for inline shader code
    Shader();

    // Destructor
    ~Shader();

    // Load shaders from files
    bool LoadFromFile(const char* vertexPath, const char* fragmentPath, const char* geometryPath = nullptr);

    // Load shaders from strings
    bool LoadFromString(const std::string& vertexCode, const std::string& fragmentCode, const std::string& geometryCode = "");

    // Activate the shader
    void Use() const;

    // Accessor to retrieve shader program ID
    unsigned int GetProgramID() const { return ID; }

    // Utility uniform functions
    void SetBool(const std::string& name, bool value);
    void SetInt(const std::string& name, int value);
    void SetFloat(const std::string& name, float value);
    void SetVec2(const std::string& name, const glm::vec2& value);
    void SetVec2(const std::string& name, float x, float y);
    void SetVec3(const std::string& name, const glm::vec3& value);
    void SetVec3(const std::string& name, float x, float y, float z);
    void SetVec4(const std::string& name, const glm::vec4& value);
    void SetVec4(const std::string& name, float x, float y, float z, float w);
    void SetMat2(const std::string& name, const glm::mat2& mat);
    void SetMat3(const std::string& name, const glm::mat3& mat);
    void SetMat4(const std::string& name, const glm::mat4& mat);

    // Lighting utility functions
    void SetLight(const std::string& baseName, const glm::vec3& position, const glm::vec3& color, float intensity = 1.0f);
    void SetMaterial(float ambient, float diffuse, float specular, float shininess);

    // Advanced uniform functions
    void SetFloatArray(const std::string& name, float* values, int count);
    void SetIntArray(const std::string& name, int* values, int count);

    // Validation and debugging
    bool IsValid() const { return ID != 0; }
    void PrintActiveUniforms() const;
    void PrintActiveAttributes() const;

    // Hot reloading (useful for development)
    bool Reload();

    // Print the file paths this shader was loaded from (if any)
    void PrintSourceFilePaths() const;

private:
    // Store file paths for hot reloading
    std::string vertexPath_;
    std::string fragmentPath_;
    std::string geometryPath_;

    // Uniform location cache for performance
    mutable std::unordered_map<std::string, int> uniformLocationCache_;

    // Utility function for checking shader compilation/linking errors
    void CheckCompileErrors(GLuint shader, const std::string& type) const;

    // Get uniform location with caching
    int GetUniformLocation(const std::string& name) const;

    // Compile shader from source
    unsigned int CompileShader(const std::string& source, GLenum type) const;

    // Link shader program
    bool LinkProgram(unsigned int vertex, unsigned int fragment, unsigned int geometry = 0);

    // Read file content
    std::string ReadFile(const std::string& filePath) const;
};

// Shader Manager class for managing multiple shaders
class ShaderManager
{
public:
    // Singleton pattern
    static ShaderManager& Instance();

    // Load and store a shader
    bool LoadShader(const std::string& name, const char* vertexPath, const char* fragmentPath, const char* geometryPath = nullptr);

    // Get a shader by name
    Shader* GetShader(const std::string& name);

    // Use a shader by name
    void UseShader(const std::string& name);

    // Reload all shaders (useful for development)
    void ReloadAll();

    // Clean up all shaders
    void CleanUp();

    // Get default shaders
    Shader* GetBasicShader();
    Shader* GetLightingShader();

private:
    ShaderManager() = default;
    ~ShaderManager();

    // Delete copy constructor and assignment operator
    ShaderManager(const ShaderManager&) = delete;
    ShaderManager& operator=(const ShaderManager&) = delete;

    std::unordered_map<std::string, std::unique_ptr<Shader>> shaders_;
    bool defaultShadersCreated_ = false;

    // Create default shaders
    void CreateDefaultShaders();
};