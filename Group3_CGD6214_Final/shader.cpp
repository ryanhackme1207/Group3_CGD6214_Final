#include "Shader.h"
#include <memory>

// Constructor reads and builds the shader
Shader::Shader(const char* vertexPath, const char* fragmentPath, const char* geometryPath)
    : ID(0)
{
    LoadFromFile(vertexPath, fragmentPath, geometryPath);
}

// Default constructor
Shader::Shader() : ID(0) {}

// Destructor
Shader::~Shader()
{
    if (ID != 0) {
        glDeleteProgram(ID);
    }
}

// Load shaders from files
bool Shader::LoadFromFile(const char* vertexPath, const char* fragmentPath, const char* geometryPath)
{
    // Store paths for hot reloading
    vertexPath_ = vertexPath;
    fragmentPath_ = fragmentPath;
    if (geometryPath) geometryPath_ = geometryPath;

    try {
        // Read vertex shader
        std::string vertexCode = ReadFile(vertexPath);
        if (vertexCode.empty()) {
            std::cerr << "ERROR::SHADER::VERTEX_FILE_NOT_READ: " << vertexPath << std::endl;
            return false;
        }

        // Read fragment shader
        std::string fragmentCode = ReadFile(fragmentPath);
        if (fragmentCode.empty()) {
            std::cerr << "ERROR::SHADER::FRAGMENT_FILE_NOT_READ: " << fragmentPath << std::endl;
            return false;
        }

        // Read geometry shader if provided
        std::string geometryCode;
        if (geometryPath) {
            geometryCode = ReadFile(geometryPath);
        }

        return LoadFromString(vertexCode, fragmentCode, geometryCode);
    }
    catch (std::exception& e) {
        std::cerr << "ERROR::SHADER::FILE_READ_FAILED: " << e.what() << std::endl;
        return false;
    }
}

// Load shaders from strings
bool Shader::LoadFromString(const std::string& vertexCode, const std::string& fragmentCode, const std::string& geometryCode)
{
    // Delete existing program if it exists
    if (ID != 0) {
        glDeleteProgram(ID);
        ID = 0;
        uniformLocationCache_.clear();
    }

    // Compile vertex shader
    unsigned int vertex = CompileShader(vertexCode, GL_VERTEX_SHADER);
    if (vertex == 0) return false;

    // Compile fragment shader
    unsigned int fragment = CompileShader(fragmentCode, GL_FRAGMENT_SHADER);
    if (fragment == 0) {
        glDeleteShader(vertex);
        return false;
    }

    // Compile geometry shader if provided
    unsigned int geometry = 0;
    if (!geometryCode.empty()) {
        geometry = CompileShader(geometryCode, GL_GEOMETRY_SHADER);
        if (geometry == 0) {
            glDeleteShader(vertex);
            glDeleteShader(fragment);
            return false;
        }
    }

    // Link shaders
    bool success = LinkProgram(vertex, fragment, geometry);

    // Clean up individual shaders
    glDeleteShader(vertex);
    glDeleteShader(fragment);
    if (geometry != 0) glDeleteShader(geometry);

    return success;
}

// Activate the shader
void Shader::Use() const
{
    if (ID != 0) {
        glUseProgram(ID);
    }
}

// Utility functions for setting uniforms
void Shader::SetBool(const std::string& name, bool value)
{
    glUniform1i(GetUniformLocation(name), static_cast<int>(value));
}

void Shader::SetInt(const std::string& name, int value)
{
    glUniform1i(GetUniformLocation(name), value);
}

void Shader::SetFloat(const std::string& name, float value)
{
    glUniform1f(GetUniformLocation(name), value);
}

void Shader::SetVec2(const std::string& name, const glm::vec2& value)
{
    glUniform2fv(GetUniformLocation(name), 1, &value[0]);
}

void Shader::SetVec2(const std::string& name, float x, float y)
{
    glUniform2f(GetUniformLocation(name), x, y);
}

void Shader::SetVec3(const std::string& name, const glm::vec3& value)
{
    glUniform3fv(GetUniformLocation(name), 1, &value[0]);
}

void Shader::SetVec3(const std::string& name, float x, float y, float z)
{
    glUniform3f(GetUniformLocation(name), x, y, z);
}

void Shader::SetVec4(const std::string& name, const glm::vec4& value)
{
    glUniform4fv(GetUniformLocation(name), 1, &value[0]);
}

void Shader::SetVec4(const std::string& name, float x, float y, float z, float w)
{
    glUniform4f(GetUniformLocation(name), x, y, z, w);
}

void Shader::SetMat2(const std::string& name, const glm::mat2& mat)
{
    glUniformMatrix2fv(GetUniformLocation(name), 1, GL_FALSE, &mat[0][0]);
}

void Shader::SetMat3(const std::string& name, const glm::mat3& mat)
{
    glUniformMatrix3fv(GetUniformLocation(name), 1, GL_FALSE, &mat[0][0]);
}

void Shader::SetMat4(const std::string& name, const glm::mat4& mat)
{
    glUniformMatrix4fv(GetUniformLocation(name), 1, GL_FALSE, &mat[0][0]);
}

// Lighting utility functions
void Shader::SetLight(const std::string& baseName, const glm::vec3& position, const glm::vec3& color, float intensity)
{
    SetVec3(baseName + ".position", position);
    SetVec3(baseName + ".color", color * intensity);
    SetFloat(baseName + ".intensity", intensity);
}

void Shader::SetMaterial(float ambient, float diffuse, float specular, float shininess)
{
    SetFloat("material.ambient", ambient);
    SetFloat("material.diffuse", diffuse);
    SetFloat("material.specular", specular);
    SetFloat("material.shininess", shininess);
}

// Array uniform functions
void Shader::SetFloatArray(const std::string& name, float* values, int count)
{
    glUniform1fv(GetUniformLocation(name), count, values);
}

void Shader::SetIntArray(const std::string& name, int* values, int count)
{
    glUniform1iv(GetUniformLocation(name), count, values);
}

// Hot reloading
bool Shader::Reload()
{
    if (vertexPath_.empty() || fragmentPath_.empty()) {
        std::cerr << "ERROR::SHADER::RELOAD: No file paths stored" << std::endl;
        return false;
    }

    std::cout << "Reloading shader: " << vertexPath_ << ", " << fragmentPath_ << std::endl;
    return LoadFromFile(vertexPath_.c_str(), fragmentPath_.c_str(),
        geometryPath_.empty() ? nullptr : geometryPath_.c_str());
}

// Print active uniforms (for debugging)
void Shader::PrintActiveUniforms() const
{
    if (ID == 0) return;

    int count;
    glGetProgramiv(ID, GL_ACTIVE_UNIFORMS, &count);
    std::cout << "Active uniforms for shader " << ID << ":" << std::endl;

    for (int i = 0; i < count; i++) {
        char name[256];
        int length, size;
        GLenum type;
        glGetActiveUniform(ID, i, sizeof(name), &length, &size, &type, name);
        std::cout << "  " << i << ": " << name << std::endl;
    }
}

// Print active attributes (for debugging)
void Shader::PrintActiveAttributes() const
{
    if (ID == 0) return;

    int count;
    glGetProgramiv(ID, GL_ACTIVE_ATTRIBUTES, &count);
    std::cout << "Active attributes for shader " << ID << ":" << std::endl;

    for (int i = 0; i < count; i++) {
        char name[256];
        int length, size;
        GLenum type;
        glGetActiveAttrib(ID, i, sizeof(name), &length, &size, &type, name);
        std::cout << "  " << i << ": " << name << std::endl;
    }
}

// Private helper functions
void Shader::CheckCompileErrors(GLuint shader, const std::string& type) const
{
    int success;
    char infoLog[1024];

    if (type != "PROGRAM") {
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (!success) {
            glGetShaderInfoLog(shader, 1024, NULL, infoLog);
            std::cerr << "ERROR::SHADER_COMPILATION_ERROR of type: " << type << "\n" << infoLog
                << "\n -- --------------------------------------------------- -- " << std::endl;
        }
    }
    else {
        glGetProgramiv(shader, GL_LINK_STATUS, &success);
        if (!success) {
            glGetProgramInfoLog(shader, 1024, NULL, infoLog);
            std::cerr << "ERROR::PROGRAM_LINKING_ERROR of type: " << type << "\n" << infoLog
                << "\n -- --------------------------------------------------- -- " << std::endl;
        }
    }
}

int Shader::GetUniformLocation(const std::string& name) const
{
    // Check cache first
    auto it = uniformLocationCache_.find(name);
    if (it != uniformLocationCache_.end()) {
        return it->second;
    }

    // Get location and cache it
    int location = glGetUniformLocation(ID, name.c_str());
    uniformLocationCache_[name] = location;

    if (location == -1) {
        std::cerr << "Warning: uniform '" << name << "' not found in shader " << ID << std::endl;
    }

    return location;
}

unsigned int Shader::CompileShader(const std::string& source, GLenum type) const
{
    unsigned int shader = glCreateShader(type);
    const char* src = source.c_str();
    glShaderSource(shader, 1, &src, NULL);
    glCompileShader(shader);

    std::string typeName;
    switch (type) {
    case GL_VERTEX_SHADER: typeName = "VERTEX"; break;
    case GL_FRAGMENT_SHADER: typeName = "FRAGMENT"; break;
    case GL_GEOMETRY_SHADER: typeName = "GEOMETRY"; break;
    default: typeName = "UNKNOWN"; break;
    }

    CheckCompileErrors(shader, typeName);

    // Check if compilation was successful
    int success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glDeleteShader(shader);
        return 0;
    }

    return shader;
}

bool Shader::LinkProgram(unsigned int vertex, unsigned int fragment, unsigned int geometry)
{
    ID = glCreateProgram();
    glAttachShader(ID, vertex);
    glAttachShader(ID, fragment);
    if (geometry != 0) {
        glAttachShader(ID, geometry);
    }

    glLinkProgram(ID);
    CheckCompileErrors(ID, "PROGRAM");

    // Check if linking was successful
    int success;
    glGetProgramiv(ID, GL_LINK_STATUS, &success);
    if (!success) {
        glDeleteProgram(ID);
        ID = 0;
        return false;
    }

    return true;
}

std::string Shader::ReadFile(const std::string& filePath) const
{
    std::ifstream file;
    std::stringstream stream;

    // Ensure ifstream objects can throw exceptions
    file.exceptions(std::ifstream::failbit | std::ifstream::badbit);

    try {
        file.open(filePath);
        stream << file.rdbuf();
        file.close();
        return stream.str();
    }
    catch (std::ifstream::failure& e) {
        std::cerr << "ERROR::SHADER::FILE_NOT_SUCCESSFULLY_READ: " << filePath << std::endl;
        return "";
    }
}

// ShaderManager implementation
ShaderManager& ShaderManager::Instance()
{
    static ShaderManager instance;
    return instance;
}

ShaderManager::~ShaderManager()
{
    CleanUp();
}

bool ShaderManager::LoadShader(const std::string& name, const char* vertexPath, const char* fragmentPath, const char* geometryPath)
{
    auto shader = std::make_unique<Shader>(vertexPath, fragmentPath, geometryPath);
    if (!shader->IsValid()) {
        std::cerr << "Failed to load shader: " << name << std::endl;
        return false;
    }

    shaders_[name] = std::move(shader);
    std::cout << "Loaded shader: " << name << std::endl;
    return true;
}

Shader* ShaderManager::GetShader(const std::string& name)
{
    auto it = shaders_.find(name);
    if (it != shaders_.end()) {
        return it->second.get();
    }

    std::cerr << "Shader not found: " << name << std::endl;
    return nullptr;
}

void ShaderManager::UseShader(const std::string& name)
{
    Shader* shader = GetShader(name);
    if (shader) {
        shader->Use();
    }
}

void ShaderManager::ReloadAll()
{
    std::cout << "Reloading all shaders..." << std::endl;
    for (auto& pair : shaders_) {
        pair.second->Reload();
    }
}

void ShaderManager::CleanUp()
{
    shaders_.clear();
}

Shader* ShaderManager::GetBasicShader()
{
    if (!defaultShadersCreated_) {
        CreateDefaultShaders();
    }
    return GetShader("basic");
}

Shader* ShaderManager::GetLightingShader()
{
    if (!defaultShadersCreated_) {
        CreateDefaultShaders();
    }
    return GetShader("lighting");
}

void ShaderManager::CreateDefaultShaders()
{
    // Basic shader (simple color)
    std::string basicVert = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoord;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

out vec3 FragPos;
out vec3 Normal;
out vec2 TexCoord;

void main()
{
    FragPos = vec3(model * vec4(aPos, 1.0));
    Normal = mat3(transpose(inverse(model))) * aNormal;
    TexCoord = aTexCoord;
    
    gl_Position = projection * view * vec4(FragPos, 1.0);
}
)";

    std::string basicFrag = R"(
#version 330 core
out vec4 FragColor;

uniform vec3 objectColor;

void main()
{
    FragColor = vec4(objectColor, 1.0);
}
)";

    // Lighting shader (Phong lighting)
    std::string lightingVert = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoord;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

out vec3 FragPos;
out vec3 Normal;
out vec2 TexCoord;

void main()
{
    FragPos = vec3(model * vec4(aPos, 1.0));
    Normal = mat3(transpose(inverse(model))) * aNormal;
    TexCoord = aTexCoord;
    
    gl_Position = projection * view * vec4(FragPos, 1.0);
}
)";

    std::string lightingFrag = R"(
#version 330 core
out vec4 FragColor;

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoord;

uniform vec3 lightPos;
uniform vec3 lightColor;
uniform vec3 objectColor;
uniform vec3 viewPos;

void main()
{
    // Ambient
    float ambientStrength = 0.1;
    vec3 ambient = ambientStrength * lightColor;
    
    // Diffuse
    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(lightPos - FragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * lightColor;
    
    // Specular
    float specularStrength = 0.5;
    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 reflectDir = reflect(-lightDir, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);
    vec3 specular = specularStrength * spec * lightColor;
    
    vec3 result = (ambient + diffuse + specular) * objectColor;
    FragColor = vec4(result, 1.0);
}
)";

    // Create default shaders
    auto basicShader = std::make_unique<Shader>();
    basicShader->LoadFromString(basicVert, basicFrag);
    shaders_["basic"] = std::move(basicShader);

    auto lightingShader = std::make_unique<Shader>();
    lightingShader->LoadFromString(lightingVert, lightingFrag);
    shaders_["lighting"] = std::move(lightingShader);

    defaultShadersCreated_ = true;
    std::cout << "Created default shaders" << std::endl;
}