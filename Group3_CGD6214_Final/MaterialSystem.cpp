// MaterialSystem.cpp Implementation
#include "MaterialSystem.h"
#include <iostream>
#include <GL/glew.h>
#include <glm/gtc/type_ptr.hpp>
#include "stb_image.h"

Material::Material(const std::string& materialName)
    : name(materialName), albedo(1.0f), metallic(0.0f), roughness(0.5f), ao(1.0f),
    emission(0.0f), emissionStrength(0.0f), ambient(0.1f), diffuse(0.8f), specular(1.0f),
    shininess(32.0f), reflectivity(0.0f), transparency(0.0f),
    animatedTextures(false), textureScrollSpeed(1.0f), textureOffset(0.0f) {
}

Material::~Material() {
    // Textures are managed by MaterialManager, don't delete here
}

bool Material::loadDiffuseMap(const std::string& path) {
    diffuseMap.id = loadTextureFromFile(path);
    if (diffuseMap.id != 0) {
        diffuseMap.path = path;
        diffuseMap.loaded = true;
        return true;
    }
    return false;
}

bool Material::loadSpecularMap(const std::string& path) {
    specularMap.id = loadTextureFromFile(path);
    if (specularMap.id != 0) {
        specularMap.path = path;
        specularMap.loaded = true;
        return true;
    }
    return false;
}

bool Material::loadNormalMap(const std::string& path) {
    normalMap.id = loadTextureFromFile(path, false); // Normal maps shouldn't be gamma corrected
    if (normalMap.id != 0) {
        normalMap.path = path;
        normalMap.loaded = true;
        return true;
    }
    return false;
}

bool Material::loadEmissionMap(const std::string& path) {
    emissionMap.id = loadTextureFromFile(path);
    if (emissionMap.id != 0) {
        emissionMap.path = path;
        emissionMap.loaded = true;
        return true;
    }
    return false;
}

bool Material::loadHeightMap(const std::string& path) {
    heightMap.id = loadTextureFromFile(path, false);
    if (heightMap.id != 0) {
        heightMap.path = path;
        heightMap.loaded = true;
        return true;
    }
    return false;
}

bool Material::loadAOMap(const std::string& path) {
    aoMap.id = loadTextureFromFile(path, false);
    if (aoMap.id != 0) {
        aoMap.path = path;
        aoMap.loaded = true;
        return true;
    }
    return false;
}

bool Material::loadMetallicMap(const std::string& path) {
    metallicMap.id = loadTextureFromFile(path, false);
    if (metallicMap.id != 0) {
        metallicMap.path = path;
        metallicMap.loaded = true;
        return true;
    }
    return false;
}

bool Material::loadRoughnessMap(const std::string& path) {
    roughnessMap.id = loadTextureFromFile(path, false);
    if (roughnessMap.id != 0) {
        roughnessMap.path = path;
        roughnessMap.loaded = true;
        return true;
    }
    return false;
}

void Material::updateTextureAnimation(float deltaTime) {
    if (animatedTextures) {
        textureOffset.x += textureScrollSpeed * deltaTime;
        textureOffset.y += textureScrollSpeed * deltaTime * 0.5f; // Different scroll speed for Y

        // Wrap around
        if (textureOffset.x > 1.0f) textureOffset.x -= 1.0f;
        if (textureOffset.y > 1.0f) textureOffset.y -= 1.0f;
    }
}

void Material::bindToShader(unsigned int shaderProgram) const {
    glUseProgram(shaderProgram);

    // Set material properties
    glUniform3fv(glGetUniformLocation(shaderProgram, "material.albedo"), 1, glm::value_ptr(albedo));
    glUniform1f(glGetUniformLocation(shaderProgram, "material.metallic"), metallic);
    glUniform1f(glGetUniformLocation(shaderProgram, "material.roughness"), roughness);
    glUniform1f(glGetUniformLocation(shaderProgram, "material.ao"), ao);
    glUniform3fv(glGetUniformLocation(shaderProgram, "material.emission"), 1, glm::value_ptr(emission));
    glUniform1f(glGetUniformLocation(shaderProgram, "material.emissionStrength"), emissionStrength);

    // Phong properties
    glUniform3fv(glGetUniformLocation(shaderProgram, "material.ambient"), 1, glm::value_ptr(ambient));
    glUniform3fv(glGetUniformLocation(shaderProgram, "material.diffuse"), 1, glm::value_ptr(diffuse));
    glUniform3fv(glGetUniformLocation(shaderProgram, "material.specular"), 1, glm::value_ptr(specular));
    glUniform1f(glGetUniformLocation(shaderProgram, "material.shininess"), shininess);
    glUniform1f(glGetUniformLocation(shaderProgram, "material.reflectivity"), reflectivity);
    glUniform1f(glGetUniformLocation(shaderProgram, "material.transparency"), transparency);

    // Texture availability flags
    glUniform1i(glGetUniformLocation(shaderProgram, "material.hasDiffuseMap"), diffuseMap.loaded);
    glUniform1i(glGetUniformLocation(shaderProgram, "material.hasSpecularMap"), specularMap.loaded);
    glUniform1i(glGetUniformLocation(shaderProgram, "material.hasNormalMap"), normalMap.loaded);
    glUniform1i(glGetUniformLocation(shaderProgram, "material.hasEmissionMap"), emissionMap.loaded);

    // Animation properties
    glUniform2fv(glGetUniformLocation(shaderProgram, "material.textureOffset"), 1, glm::value_ptr(textureOffset));

    // Set texture samplers
    glUniform1i(glGetUniformLocation(shaderProgram, "material.diffuseMap"), 0);
    glUniform1i(glGetUniformLocation(shaderProgram, "material.specularMap"), 1);
    glUniform1i(glGetUniformLocation(shaderProgram, "material.normalMap"), 2);
    glUniform1i(glGetUniformLocation(shaderProgram, "material.emissionMap"), 3);
}

void Material::bindTextures() const {
    if (diffuseMap.loaded) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, diffuseMap.id);
    }
    if (specularMap.loaded) {
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, specularMap.id);
    }
    if (normalMap.loaded) {
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, normalMap.id);
    }
    if (emissionMap.loaded) {
        glActiveTexture(GL_TEXTURE3);
        glBindTexture(GL_TEXTURE_2D, emissionMap.id);
    }
}

void Material::unbindTextures() const {
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, 0);
}

unsigned int Material::loadTextureFromFile(const std::string& path, bool gammaCorrection) {
    unsigned int textureID;
    glGenTextures(1, &textureID);

    int width, height, nrChannels;
    unsigned char* data = stbi_load(path.c_str(), &width, &height, &nrChannels, 0);
    if (data) {
        GLenum internalFormat, dataFormat;
        if (nrChannels == 1) {
            internalFormat = dataFormat = GL_RED;
        }
        else if (nrChannels == 3) {
            internalFormat = gammaCorrection ? GL_SRGB : GL_RGB;
            dataFormat = GL_RGB;
        }
        else if (nrChannels == 4) {
            internalFormat = gammaCorrection ? GL_SRGB_ALPHA : GL_RGBA;
            dataFormat = GL_RGBA;
        }

        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, dataFormat, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);

        // Set texture parameters for high quality
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        // Enable anisotropic filtering if available (check extension first)
        if (glewIsSupported("GL_EXT_texture_filter_anisotropic")) {
            float maxAnisotropy;
            glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &maxAnisotropy);
            glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, maxAnisotropy);
        }

        stbi_image_free(data);
        std::cout << "Loaded texture: " << path << " (" << width << "x" << height << ", " << nrChannels << " channels)" << std::endl;
    }
    else {
        std::cout << "Failed to load texture: " << path << std::endl;
        stbi_image_free(data);
        return 0;
    }

    return textureID;
}

// MaterialManager Implementation
MaterialManager::MaterialManager() {
    initializePredefinedMaterials();
}

MaterialManager::~MaterialManager() {
    clearAllMaterials();
    clearTextureCache();
}

Material* MaterialManager::createMaterial(const std::string& name) {
    auto it = materials.find(name);
    if (it != materials.end()) {
        return it->second.get();
    }

    auto material = std::make_unique<Material>(name);
    Material* ptr = material.get();
    materials[name] = std::move(material);
    return ptr;
}

Material* MaterialManager::getMaterial(const std::string& name) {
    auto it = materials.find(name);
    return (it != materials.end()) ? it->second.get() : nullptr;
}

bool MaterialManager::removeMaterial(const std::string& name) {
    auto it = materials.find(name);
    if (it != materials.end()) {
        materials.erase(it);
        return true;
    }
    return false;
}

Material* MaterialManager::createBuildingMaterial(const std::string& name, const glm::vec3& color) {
    Material* mat = createMaterial(name);
    mat->setAlbedo(color);
    mat->setDiffuse(color);
    mat->setRoughness(0.8f);
    mat->setMetallic(0.0f);
    mat->setShininess(16.0f);
    return mat;
}

Material* MaterialManager::createMetalMaterial(const std::string& name, const glm::vec3& color, float roughness) {
    Material* mat = createMaterial(name);
    mat->setAlbedo(color);
    mat->setDiffuse(color);
    mat->setSpecular(glm::vec3(1.0f));
    mat->setRoughness(roughness);
    mat->setMetallic(1.0f);
    mat->setShininess(128.0f);
    mat->setReflectivity(0.3f);
    return mat;
}

Material* MaterialManager::createGlassMaterial(const std::string& name, const glm::vec3& color, float transparency) {
    Material* mat = createMaterial(name);
    mat->setAlbedo(color);
    mat->setDiffuse(color);
    mat->setSpecular(glm::vec3(1.0f));
    mat->setRoughness(0.1f);
    mat->setMetallic(0.0f);
    mat->setShininess(256.0f);
    mat->setTransparency(transparency);
    mat->setReflectivity(0.8f);
    return mat;
}

// Add the missing createConcreteMaterial method
Material* MaterialManager::createConcreteMaterial(const std::string& name) {
    Material* mat = createMaterial(name);
    mat->setAlbedo(glm::vec3(0.6f, 0.6f, 0.6f));
    mat->setDiffuse(glm::vec3(0.6f, 0.6f, 0.6f));
    mat->setSpecular(glm::vec3(0.1f, 0.1f, 0.1f));
    mat->setRoughness(0.9f);
    mat->setMetallic(0.0f);
    mat->setShininess(4.0f);
    mat->setAmbient(glm::vec3(0.2f, 0.2f, 0.2f));
    return mat;
}

Material* MaterialManager::createWoodMaterial(const std::string& name) {
    Material* mat = createMaterial(name);
    mat->setAlbedo(glm::vec3(0.6f, 0.4f, 0.2f));
    mat->setDiffuse(glm::vec3(0.6f, 0.4f, 0.2f));
    mat->setSpecular(glm::vec3(0.3f, 0.2f, 0.1f));
    mat->setRoughness(0.7f);
    mat->setMetallic(0.0f);
    mat->setShininess(8.0f);
    return mat;
}

Material* MaterialManager::createVegetationMaterial(const std::string& name) {
    Material* mat = createMaterial(name);
    mat->setAlbedo(glm::vec3(0.2f, 0.6f, 0.1f));
    mat->setDiffuse(glm::vec3(0.2f, 0.6f, 0.1f));
    mat->setRoughness(0.9f);
    mat->setMetallic(0.0f);
    mat->setShininess(8.0f);
    return mat;
}

Material* MaterialManager::createRoadMaterial(const std::string& name) {
    Material* mat = createMaterial(name);
    mat->setAlbedo(glm::vec3(0.3f, 0.3f, 0.3f));
    mat->setDiffuse(glm::vec3(0.3f, 0.3f, 0.3f));
    mat->setSpecular(glm::vec3(0.1f, 0.1f, 0.1f));
    mat->setRoughness(0.95f);
    mat->setMetallic(0.0f);
    mat->setShininess(2.0f);
    return mat;
}

Material* MaterialManager::createCarMaterial(const std::string& name, const glm::vec3& color) {
    Material* mat = createMaterial(name);
    mat->setAlbedo(color);
    mat->setDiffuse(color);
    mat->setSpecular(glm::vec3(0.8f, 0.8f, 0.8f));
    mat->setRoughness(0.3f);
    mat->setMetallic(0.8f);
    mat->setShininess(64.0f);
    mat->setReflectivity(0.4f);
    return mat;
}

Material* MaterialManager::createEmissiveMaterial(const std::string& name, const glm::vec3& emissionColor) {
    Material* mat = createMaterial(name);
    mat->setEmission(emissionColor, 2.0f);
    mat->setAlbedo(emissionColor * 0.2f);
    return mat;
}

Material* MaterialManager::createTextureAtlasMaterial(const std::string& name, const std::string& atlasPath, int tilesX, int tilesY) {
    Material* mat = createMaterial(name);
    mat->loadDiffuseMap(atlasPath);
    // Additional atlas-specific setup would go here
    return mat;
}

void MaterialManager::preloadTextures(const std::vector<std::string>& texturePaths) {
    for (const auto& path : texturePaths) {
        if (textureCache.find(path) == textureCache.end()) {
            TextureInfo info;
            info.id = Material::loadTextureFromFile(path);
            if (info.id != 0) {
                info.path = path;
                info.loaded = true;
                textureCache[path] = info;
            }
        }
    }
}

size_t MaterialManager::getTextureMemoryUsage() const {
    size_t totalMemory = 0;
    for (const auto& pair : textureCache) {
        if (pair.second.loaded) {
            // Estimate memory usage (width * height * channels * bytes per channel)
            totalMemory += pair.second.width * pair.second.height * pair.second.channels;
        }
    }
    return totalMemory;
}

void MaterialManager::initializePredefinedMaterials() {
    // Create some basic materials
    createConcreteMaterial("concrete");
    createBuildingMaterial("brick", glm::vec3(0.8f, 0.4f, 0.2f));
    createMetalMaterial("steel", glm::vec3(0.6f, 0.6f, 0.6f), 0.2f);
    createMetalMaterial("metal", glm::vec3(0.7f, 0.7f, 0.8f), 0.2f);
    createGlassMaterial("window", glm::vec3(0.8f, 0.9f, 1.0f), 0.8f);
    createGlassMaterial("glass", glm::vec3(0.8f, 0.9f, 1.0f), 0.8f);
    createVegetationMaterial("grass");
    createVegetationMaterial("tree");
    createVegetationMaterial("vegetation");
    createRoadMaterial("asphalt");
    createEmissiveMaterial("streetLight", glm::vec3(1.0f, 0.8f, 0.6f));
    createWoodMaterial("wood");
}

void MaterialManager::clearAllMaterials() {
    materials.clear();
}

void MaterialManager::clearTextureCache() {
    for (auto& pair : textureCache) {
        if (pair.second.id != 0) {
            glDeleteTextures(1, &pair.second.id);
        }
    }
    textureCache.clear();
}