#include "Model.h"
#include "Mesh.h"
#include "Shader.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <unordered_map>
#include <array>
#include <chrono>

#include "stb_image.h"

Model::Model() {}
Model::~Model() {}

static std::string GetDirectory(const std::string& path) {
    size_t pos = path.find_last_of("/\\");
    if (pos == std::string::npos) return std::string();
    return path.substr(0, pos + 1);
}

// helper: next power of two (not strictly required but useful)
static int NextPow2(int v) {
    int p = 1; while (p < v) p <<= 1; return p;
}

bool Model::LoadOBJ(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "Failed to open OBJ: " << path << std::endl;
        return false;
    }

    std::vector<glm::vec3> positions;
    std::vector<glm::vec3> normals;
    std::vector<glm::vec2> texcoords;

    // For multi-material support: map material name -> vertex/indices arrays
    std::unordered_map<std::string, std::vector<float>> matVertices;
    std::unordered_map<std::string, std::vector<unsigned int>> matIndices;
    std::vector<std::string> materialOrder; // keep order for deterministic mesh creation

    std::string currentMaterial = "__default";
    matVertices[currentMaterial] = std::vector<float>();
    matIndices[currentMaterial] = std::vector<unsigned int>();
    materialOrder.push_back(currentMaterial);

    std::string line;
    std::string mtlFileName;
    while (std::getline(file, line)) {
        std::istringstream iss(line);
        std::string prefix;
        if (!(iss >> prefix)) continue;
        if (prefix == "mtllib") {
            iss >> mtlFileName;
            std::cout << "Found mtllib: " << mtlFileName << std::endl;
        }
        else if (prefix == "usemtl") {
            iss >> currentMaterial;
            if (currentMaterial.empty()) currentMaterial = "__default";
            if (matVertices.find(currentMaterial) == matVertices.end()) {
                matVertices[currentMaterial] = std::vector<float>();
                matIndices[currentMaterial] = std::vector<unsigned int>();
                materialOrder.push_back(currentMaterial);
            }
        }
        else if (prefix == "v") {
            glm::vec3 v; iss >> v.x >> v.y >> v.z; positions.push_back(v);
        }
        else if (prefix == "vn") {
            glm::vec3 n; iss >> n.x >> n.y >> n.z; normals.push_back(n);
        }
        else if (prefix == "vt") {
            glm::vec2 t; iss >> t.x >> t.y; texcoords.push_back(t);
        }
        else if (prefix == "f") {
            std::string a, b, c;
            iss >> a >> b >> c;
            auto parseFaceToMat = [&](const std::string& token, std::vector<float>& outVerts, std::vector<unsigned int>& outInds) {
                unsigned int vi = 0, ti = 0, ni = 0;
                size_t p1 = token.find('/');
                size_t p2 = std::string::npos;
                if (p1 != std::string::npos) p2 = token.find('/', p1 + 1);
                if (p1 != std::string::npos && p2 != std::string::npos) {
                    std::string sv = token.substr(0, p1);
                    std::string st = token.substr(p1 + 1, p2 - p1 - 1);
                    std::string sn = token.substr(p2 + 1);
                    try {
                        if (!sv.empty()) vi = static_cast<unsigned int>(std::stoul(sv));
                        if (!st.empty()) ti = static_cast<unsigned int>(std::stoul(st));
                        if (!sn.empty()) ni = static_cast<unsigned int>(std::stoul(sn));
                    }
                    catch (...) { return; }
                }
                else {
                    try { vi = static_cast<unsigned int>(std::stoul(token)); }
                    catch (...) { return; }
                }

                if (vi == 0 || vi > positions.size()) return;
                glm::vec3 pos = positions[vi - 1];
                glm::vec2 uv = (ti > 0 && ti <= texcoords.size()) ? texcoords[ti - 1] : glm::vec2(0.0f);
                glm::vec3 nrm = (ni > 0 && ni <= normals.size()) ? normals[ni - 1] : glm::vec3(0.0f, 1.0f, 0.0f);

                // Skip faces that are part of the "3 rods top" by filtering faces with all vertices above a Y threshold
                // We'll detect "visiongt" filename later; for now append and we'll filter when creating meshes.
                outVerts.push_back(pos.x); outVerts.push_back(pos.y); outVerts.push_back(pos.z);
                outVerts.push_back(nrm.x); outVerts.push_back(nrm.y); outVerts.push_back(nrm.z);
                outVerts.push_back(uv.x); outVerts.push_back(uv.y);
                outInds.push_back(static_cast<unsigned int>(outInds.size()));
            };

            // Append to current material's vertex/index lists
            parseFaceToMat(a, matVertices[currentMaterial], matIndices[currentMaterial]);
            parseFaceToMat(b, matVertices[currentMaterial], matIndices[currentMaterial]);
            parseFaceToMat(c, matVertices[currentMaterial], matIndices[currentMaterial]);
        }
    }

    // Decide whether to load textures: only enable for visiongt OBJ
    bool loadTextures = false;
    size_t pos = path.find_last_of("/\\");
    std::string fname = (pos == std::string::npos) ? path : path.substr(pos + 1);
    // lowercase filename for case-insensitive check
    std::string lower = fname;
    for (auto& ch : lower) ch = static_cast<char>(::tolower(ch));
    if (lower.find("visiongt") != std::string::npos) loadTextures = true;

    // Attempt to locate texture via MTL if available and map textures per material
    std::unordered_map<std::string, std::string> materialToTexture;
    if (loadTextures && !mtlFileName.empty()) {
        std::string dir = GetDirectory(path);
        std::string mtlPath = dir + mtlFileName;
        std::ifstream mtl(mtlPath);
        if (mtl.is_open()) {
            std::cout << "Parsing MTL: " << mtlPath << std::endl;
            std::string mline;
            std::string activeMat;
            while (std::getline(mtl, mline)) {
                std::istringstream miss(mline);
                std::string mprefix;
                if (!(miss >> mprefix)) continue;
                if (mprefix == "newmtl") {
                    miss >> activeMat;
                    // trim
                    if (activeMat.size() && activeMat.back() == '\r') activeMat.pop_back();
                    // debug
                    std::cout << "Found material: '" << activeMat << "'" << std::endl;
                }
                else if (mprefix == "map_Kd" || mprefix == "map_kd" || mprefix == "map_Ka") {
                    std::string texname; miss >> texname;
                    if (!texname.empty() && texname.back() == '\r') texname.pop_back();
                    if (!texname.empty()) {
                        std::string texpath = dir + texname;
                        std::ifstream ttest(texpath);
                        if (!ttest.is_open()) {
                            texpath = dir + "textures/" + texname;
                            std::ifstream ttest2(texpath);
                            if (!ttest2.is_open()) texpath.clear();
                        }
                        if (!texpath.empty() && !activeMat.empty()) {
                            materialToTexture[activeMat] = texpath;
                            std::cout << "Material '" << activeMat << "' -> texture '" << texpath << "'" << std::endl;
                        }
                        else {
                            std::cout << "Texture '" << texname << "' not found for material '" << activeMat << "'" << std::endl;
                        }
                    }
                }
            }
        }
        else {
            std::cerr << "Failed to open MTL at: " << mtlPath << " ; trying filename only: " << mtlFileName << std::endl;
            // try mtl in same working folder
            std::ifstream mtl2(mtlFileName);
            if (mtl2.is_open()) {
                std::cout << "Parsing MTL (fallback): " << mtlFileName << std::endl;
                std::string mline; std::string activeMat;
                while (std::getline(mtl2, mline)) {
                    std::istringstream miss(mline);
                    std::string mprefix; if (!(miss >> mprefix)) continue;
                    if (mprefix == "newmtl") { miss >> activeMat; if (activeMat.size() && activeMat.back() == '\r') activeMat.pop_back(); std::cout << "Found material: '" << activeMat << "'" << std::endl; }
                    else if (mprefix == "map_Kd" || mprefix == "map_kd" || mprefix == "map_Ka") {
                        std::string texname; miss >> texname; if (!texname.empty() && texname.back() == '\r') texname.pop_back();
                        if (!texname.empty() && !activeMat.empty()) {
                            std::string texpath = texname;
                            std::ifstream ttest(texpath);
                            if (!ttest.is_open()) texpath.clear();
                            if (!texpath.empty()) {
                                materialToTexture[activeMat] = texpath;
                                std::cout << "Material '" << activeMat << "' -> texture '" << texpath << "' (fallback)" << std::endl;
                            }
                        }
                    }
                }
            }
        }
    }
    else if (!loadTextures) {
        std::cout << "Skipping MTL/texture loading for: " << fname << std::endl;
    }

    // If we have textures to atlas, build atlas and remap UVs
    GLuint atlasTex = 0;
    std::unordered_map<std::string, std::array<float,4>> materialRect; // ox,oy,ow,oh
    if (loadTextures && !materialToTexture.empty()) {
        // collect unique texture paths in deterministic order
        std::vector<std::string> uniqueTexPaths;
        std::unordered_map<std::string,int> texIndex;
        for (const auto &m : materialOrder) {
            auto it = materialToTexture.find(m);
            if (it != materialToTexture.end()) {
                const std::string &p = it->second;
                if (texIndex.find(p) == texIndex.end()) {
                    texIndex[p] = (int)uniqueTexPaths.size();
                    uniqueTexPaths.push_back(p);
                }
            }
        }

        if (!uniqueTexPaths.empty()) {
            // load all images
            struct Img { int w,h,channels; std::vector<unsigned char> data; std::string path; };
            std::vector<Img> imgs; imgs.reserve(uniqueTexPaths.size());
            int maxW = 0, maxH = 0;
            stbi_set_flip_vertically_on_load(true);
            for (const auto &p : uniqueTexPaths) {
                Img im; im.path = p;
                int w,h,chn;
                unsigned char *d = stbi_load(p.c_str(), &w, &h, &chn, 0);
                if (!d) {
                    std::cerr << "Atlas: failed to load " << p << std::endl;
                    continue;
                }
                im.w = w; im.h = h; im.channels = chn;
                // convert to RGBA
                im.data.resize(w * h * 4);
                for (int y = 0; y < h; ++y) {
                    for (int x = 0; x < w; ++x) {
                        int srcIdx = (y * w + x) * chn;
                        int dstIdx = (y * w + x) * 4;
                        if (chn == 1) {
                            unsigned char v = d[srcIdx]; im.data[dstIdx+0]=v; im.data[dstIdx+1]=v; im.data[dstIdx+2]=v; im.data[dstIdx+3]=255;
                        } else if (chn == 3) {
                            im.data[dstIdx+0] = d[srcIdx+0]; im.data[dstIdx+1] = d[srcIdx+1]; im.data[dstIdx+2] = d[srcIdx+2]; im.data[dstIdx+3] = 255;
                        } else if (chn == 4) {
                            im.data[dstIdx+0] = d[srcIdx+0]; im.data[dstIdx+1] = d[srcIdx+1]; im.data[dstIdx+2] = d[srcIdx+2]; im.data[dstIdx+3] = d[srcIdx+3];
                        }
                    }
                }
                stbi_image_free(d);
                imgs.push_back(std::move(im));
                maxW = std::max(maxW, imgs.back().w);
                maxH = std::max(maxH, imgs.back().h);
            }

            if (!imgs.empty()) {
                int N = (int)imgs.size();
                int cols = (int)std::ceil(std::sqrt((double)N));
                int rows = (int)std::ceil((double)N / cols);
                int cellW = maxW;
                int cellH = maxH;
                int atlasW = cellW * cols;
                int atlasH = cellH * rows;

                std::vector<unsigned char> atlasData(atlasW * atlasH * 4);
                // initialize transparent
                std::fill(atlasData.begin(), atlasData.end(), 0);

                for (int i = 0; i < N; ++i) {
                    int col = i % cols;
                    int row = i / cols;
                    int ox = col * cellW;
                    int oy = row * cellH;
                    Img &im = imgs[i];
                    // copy image into atlas
                    for (int y = 0; y < im.h; ++y) {
                        for (int x = 0; x < im.w; ++x) {
                            int dstX = ox + x;
                            int dstY = oy + y;
                            int dstIdx = (dstY * atlasW + dstX) * 4;
                            int srcIdx = (y * im.w + x) * 4;
                            atlasData[dstIdx+0] = im.data[srcIdx+0];
                            atlasData[dstIdx+1] = im.data[srcIdx+1];
                            atlasData[dstIdx+2] = im.data[srcIdx+2];
                            atlasData[dstIdx+3] = im.data[srcIdx+3];
                        }
                    }
                    float oxN = (float)ox / (float)atlasW;
                    float oyN = (float)oy / (float)atlasH;
                    float owN = (float)im.w / (float)atlasW;
                    float ohN = (float)im.h / (float)atlasH;
                    materialRect[ uniqueTexPaths[i] ] = { oxN, oyN, owN, ohN };
                }

                // create GL texture for atlas
                glGenTextures(1, &atlasTex);
                glBindTexture(GL_TEXTURE_2D, atlasTex);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, atlasW, atlasH, 0, GL_RGBA, GL_UNSIGNED_BYTE, atlasData.data());
                glGenerateMipmap(GL_TEXTURE_2D);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glBindTexture(GL_TEXTURE_2D, 0);

                // remap UVs in matVertices according to atlas rects
                for (const auto &m : materialOrder) {
                    auto it = materialToTexture.find(m);
                    if (it == materialToTexture.end()) continue;
                    const std::string &texp = it->second;
                    auto rit = materialRect.find(texp);
                    if (rit == materialRect.end()) continue;
                    auto rect = rit->second; // ox,oy,ow,oh
                    auto &verts = matVertices[m];
                    for (size_t vi = 0; vi + 7 < verts.size(); vi += 8) {
                        float u = verts[vi + 6];
                        float v = verts[vi + 7];
                        verts[vi + 6] = rect[0] + u * rect[2];
                        verts[vi + 7] = rect[1] + v * rect[3];
                    }
                }
            }
        }
    }

    // Create meshes for each material group
    bool anyMesh = false;

    // For visiongt: determine Y threshold to remove top rods. We'll compute maxY of model and remove faces with all verts above (maxY - delta)
    float maxY = -1e9f;
    if (lower.find("visiongt") != std::string::npos) {
        // compute maxY from all positions
        for (const auto& m : matVertices) {
            const auto& verts = m.second;
            for (size_t i = 0; i + 7 < verts.size(); i += 8) {
                float y = verts[i + 1];
                if (y > maxY) maxY = y;
            }
        }
    }
    float rodCutDelta = 1.0f; // remove faces whose all vertices are within rodCutDelta of maxY

    bool atlasAssignedOwner = false;
    for (const auto& matName : materialOrder) {
        auto verts = matVertices[matName]; // copy so we can filter
        auto inds = matIndices[matName];
        if (verts.empty() || inds.empty()) continue;

        // If visiongt, filter out top rods: iterate triangles (3 vertices per face)
        if (lower.find("visiongt") != std::string::npos && maxY > -1e8f) {
            std::vector<float> newVerts;
            std::vector<unsigned int> newInds;
            // Each face we built has indices as 0..N sequential for that material. Triangles are groups of 3 indices
            size_t triCount = inds.size() / 3;
            for (size_t t = 0; t < triCount; ++t) {
                bool allHigh = true;
                std::array<glm::vec3, 3> triPos;
                for (int k = 0; k < 3; ++k) {
                    unsigned int localIndex = inds[t * 3 + k];
                    size_t vi = static_cast<size_t>(localIndex) * 8; // 8 floats per vertex
                    if (vi + 2 >= verts.size()) { allHigh = false; break; }
                    triPos[k] = glm::vec3(verts[vi + 0], verts[vi + 1], verts[vi + 2]);
                    if (triPos[k].y < maxY - rodCutDelta) allHigh = false;
                }
                if (allHigh) {
                    // skip this triangle (it's part of top rods)
                    continue;
                }
                // keep triangle: append its 3 vertices
                for (int k = 0; k < 3; ++k) {
                    unsigned int localIndex = inds[t * 3 + k];
                    size_t vi = static_cast<size_t>(localIndex) * 8;
                    for (size_t f = 0; f < 8; f++) newVerts.push_back(verts[vi + f]);
                    newInds.push_back(static_cast<unsigned int>(newInds.size()));
                }
            }
            verts.swap(newVerts);
            inds.swap(newInds);
        }

        if (verts.empty() || inds.empty()) continue;
        anyMesh = true;

        std::string texPath;
        if (loadTextures) {
            auto it = materialToTexture.find(matName);
            if (it != materialToTexture.end()) texPath = it->second;
        }

        if (!texPath.empty()) {
            std::cout << "Creating mesh for material '" << matName << "' with atlas texture" << std::endl;
            auto mesh = std::make_shared<Mesh>(verts, inds);
            if (atlasTex) {
                // assign atlas texture to mesh; let first mesh own it so atlas is freed once
                mesh->SetDiffuseTextureID(atlasTex, !atlasAssignedOwner);
                atlasAssignedOwner = true;
            }
            meshes.push_back(mesh);
        }
        else {
            std::cout << "Creating mesh for material '" << matName << "' without texture" << std::endl;
            auto mesh = std::make_shared<Mesh>(verts, inds);
            meshes.push_back(mesh);
        }
    }

    // If visiongt, also create a simple flat underglow quad mesh under the car center
    if (lower.find("visiongt") != std::string::npos) {
        // compute bounding box to place quad
        float minX = 1e9f, minY = 1e9f, minZ = 1e9f, maxX = -1e9f, maxZ = -1e9f;
        for (const auto& m : matVertices) {
            const auto& verts = m.second;
            for (size_t i = 0; i + 7 < verts.size(); i += 8) {
                float x = verts[i + 0], y = verts[i + 1], z = verts[i + 2];
                minX = std::min(minX, x); minY = std::min(minY, y); minZ = std::min(minZ, z);
                maxX = std::max(maxX, x); maxZ = std::max(maxZ, z);
            }
        }
        if (minX < maxX) {
            float centerX = 0.5f * (minX + maxX);
            float centerZ = 0.5f * (minZ + maxZ);
            float y = minY - 0.05f; // slightly below lowest vertex
            float w = (maxX - minX) * 0.9f;
            float d = (maxZ - minZ) * 0.9f;
            // create quad vertices (two triangles) with simple normals up and texcoords
            std::vector<float> uVerts = {
                centerX - w * 0.5f, y, centerZ - d * 0.5f,  0,1,0,  0.0f,0.0f,
                centerX + w * 0.5f, y, centerZ - d * 0.5f,  0,1,0,  1.0f,0.0f,
                centerX + w * 0.5f, y, centerZ + d * 0.5f,  0,1,0,  1.0f,1.0f,
                centerX - w * 0.5f, y, centerZ + d * 0.5f,  0,1,0,  0.0f,1.0f
            };
            std::vector<unsigned int> uInds = { 0,1,2, 0,2,3 };
            underglowMesh = std::make_shared<Mesh>(uVerts, uInds);
            hasUnderglow = true;
            std::cout << "Generated underglow quad for visiongt" << std::endl;
        }
    }

    // Debug: print mesh count and bounding box for loaded model
    {
        int meshCount = (int)meshes.size();
        float minX = 1e9f, minY = 1e9f, minZ = 1e9f, maxX = -1e9f, maxYb = -1e9f, maxZ = -1e9f;
        for (const auto &m : matVertices) {
            const auto &verts = m.second;
            for (size_t i = 0; i + 7 < verts.size(); i += 8) {
                float x = verts[i + 0], y = verts[i + 1], z = verts[i + 2];
                minX = std::min(minX, x); minY = std::min(minY, y); minZ = std::min(minZ, z);
                maxX = std::max(maxX, x); maxYb = std::max(maxYb, y); maxZ = std::max(maxZ, z);
            }
        }
        if (meshCount > 0) {
            std::cout << "Model load summary: meshes=" << meshCount << " bboxMin=(" << minX << "," << minY << "," << minZ << ") bboxMax=(" << maxX << "," << maxYb << "," << maxZ << ")" << std::endl;
        }
    }

    return anyMesh;
}

void Model::Draw(Shader& shader, const glm::mat4& modelMatrix) {
    for (auto& m : meshes) m->Draw(shader, modelMatrix);

    // draw underglow if present: use additive blending and set objectColor to animated RGB
    if (hasUnderglow && underglowMesh) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        auto now = std::chrono::high_resolution_clock::now();
        float t = std::chrono::duration<float>(now.time_since_epoch()).count();
        // compute RGB cycling
        glm::vec3 col;
        col.r = 0.5f + 0.5f * sin(t * 2.0f);
        col.g = 0.5f + 0.5f * sin(t * 2.0f + 2.0943951f); // +120deg
        col.b = 0.5f + 0.5f * sin(t * 2.0f + 4.1887902f); // +240deg
        shader.SetVec3("objectColor", col * 1.2f);
        // slightly raise model so won't z-fight
        glm::mat4 mm = modelMatrix;
        mm = glm::translate(mm, glm::vec3(0.0f, 0.001f, 0.0f));
        underglowMesh->Draw(shader, mm);
        glDisable(GL_BLEND);
    }
}
