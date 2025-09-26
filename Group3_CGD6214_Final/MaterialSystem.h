// MaterialSystem.h
#pragma once
#include <glm/glm.hpp>
#include <string>
#include <unordered_map>
#include <memory>
#include <GL/glew.h>

struct TextureInfo {
    unsigned int id;
    std::string path;
    int width, height, channels;
    bool loaded;

    TextureInfo() : id(0), width(0), height(0), channels(0), loaded(false) {}
};

class Material {
private:
    std::string name;

    // PBR Material properties
    glm::vec3 albedo;
    float metallic;
    float roughness;
    float ao; // Ambient occlusion
    glm::vec3 emission;
    float emissionStrength;

    // Traditional Phong properties
    glm::vec3 ambient;
    glm::vec3 diffuse;
    glm::vec3 specular;
    float shininess;
    float reflectivity;
    float transparency;

    // Texture maps
    TextureInfo diffuseMap;
    TextureInfo specularMap;
    TextureInfo normalMap;
    TextureInfo heightMap;
    TextureInfo emissionMap;
    TextureInfo aoMap;
    TextureInfo metallicMap;
    TextureInfo roughnessMap;

    // Animation properties
    bool animatedTextures;
    float textureScrollSpeed;
    glm::vec2 textureOffset;

public:
    Material(const std::string& materialName = "Default");
    ~Material();

    // Property setters
    void setAlbedo(const glm::vec3& color) { albedo = color; }
    void setMetallic(float m) { metallic = m; }
    void setRoughness(float r) { roughness = r; }
    void setAO(float ambientOcclusion) { ao = ambientOcclusion; }
    void setEmission(const glm::vec3& color, float strength = 1.0f) {
        emission = color;
        emissionStrength = strength;
    }

    // Phong properties
    void setAmbient(const glm::vec3& color) { ambient = color; }
    void setDiffuse(const glm::vec3& color) { diffuse = color; }
    void setSpecular(const glm::vec3& color) { specular = color; }
    void setShininess(float s) { shininess = s; }
    void setReflectivity(float r) { reflectivity = r; }
    void setTransparency(float t) { transparency = t; }

    // Texture loading
    bool loadDiffuseMap(const std::string& path);
    bool loadSpecularMap(const std::string& path);
    bool loadNormalMap(const std::string& path);
    bool loadHeightMap(const std::string& path);
    bool loadEmissionMap(const std::string& path);
    bool loadAOMap(const std::string& path);
    bool loadMetallicMap(const std::string& path);
    bool loadRoughnessMap(const std::string& path);

    // Animation
    void enableTextureAnimation(bool enable) { animatedTextures = enable; }
    void setTextureScrollSpeed(float speed) { textureScrollSpeed = speed; }
    void updateTextureAnimation(float deltaTime);

    // Binding for shader
    void bindToShader(unsigned int shaderProgram) const;
    void bindTextures() const;
    void unbindTextures() const;

    // Getters
    const std::string& getName() const { return name; }
    glm::vec3 getAlbedo() const { return albedo; }
    float getMetallic() const { return metallic; }
    float getRoughness() const { return roughness; }
    bool hasDiffuseMap() const { return diffuseMap.loaded; }
    bool hasSpecularMap() const { return specularMap.loaded; }
    bool hasNormalMap() const { return normalMap.loaded; }
    bool hasEmissionMap() const { return emissionMap.loaded; }

    // Static helper methods
    static unsigned int loadTextureFromFile(const std::string& path, bool gammaCorrection = false);
};

class MaterialManager {
private:
    std::unordered_map<std::string, std::unique_ptr<Material>> materials;
    std::unordered_map<std::string, TextureInfo> textureCache;

    // Predefined material types
    void initializePredefinedMaterials();

public:
    MaterialManager();
    ~MaterialManager();

    // Material management
    Material* createMaterial(const std::string& name);
    Material* getMaterial(const std::string& name);
    bool removeMaterial(const std::string& name);
    void clearAllMaterials();

    // Predefined materials
    Material* createBuildingMaterial(const std::string& name, const glm::vec3& color);
    Material* createMetalMaterial(const std::string& name, const glm::vec3& color, float roughness = 0.2f);
    Material* createGlassMaterial(const std::string& name, const glm::vec3& color, float transparency = 0.8f);
    Material* createConcreteMaterial(const std::string& name);
    Material* createWoodMaterial(const std::string& name);
    Material* createVegetationMaterial(const std::string& name);
    Material* createRoadMaterial(const std::string& name);
    Material* createCarMaterial(const std::string& name, const glm::vec3& color);
    Material* createEmissiveMaterial(const std::string& name, const glm::vec3& emissionColor);

    // Texture atlas support
    Material* createTextureAtlasMaterial(const std::string& name, const std::string& atlasPath,
        int tilesX, int tilesY);

    // Performance optimization
    void preloadTextures(const std::vector<std::string>& texturePaths);
    void clearTextureCache();
    size_t getTextureMemoryUsage() const;

    // Statistics
    size_t getMaterialCount() const { return materials.size(); }
    size_t getCachedTextureCount() const { return textureCache.size(); }
};