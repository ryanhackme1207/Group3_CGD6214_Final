// main.cpp
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm.hpp>
#include <gtc/matrix_transform.hpp>
#include <gtc/type_ptr.hpp>

#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>

// Data containers
struct Vertex {
    float x, y, z;
};

std::vector<Vertex> obj_vertices;
std::vector<Vertex> obj_normals;
std::vector<glm::vec2> obj_texcoords;

std::vector<Vertex> final_vertices;
std::vector<unsigned int> indices;

bool loadOBJ(const char* path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cout << "Failed to open OBJ file: " << path << std::endl;
        return false;
    }

    std::string line;
    std::vector<std::vector<int>> face_vertex_indices;
    std::vector<std::vector<int>> face_normal_indices;
    std::vector<std::vector<int>> face_texcoord_indices;

    while (std::getline(file, line)) {
        std::istringstream iss(line);
        std::string type;
        iss >> type;

        if (type == "v") {
            Vertex v;
            iss >> v.x >> v.y >> v.z;
            obj_vertices.push_back(v);
        }
        else if (type == "vt") {
            glm::vec2 t;
            iss >> t.x >> t.y;
            obj_texcoords.push_back(t);
        }
        else if (type == "vn") {
            Vertex n;
            iss >> n.x >> n.y >> n.z;
            obj_normals.push_back(n);
        }
        else if (type == "f") {
            std::vector<int> v_indices, vt_indices, vn_indices;
            std::string vertex_string;

            while (iss >> vertex_string) {
                std::replace(vertex_string.begin(), vertex_string.end(), '/', ' ');
                std::istringstream vertex_iss(vertex_string);

                int v_idx = 0, vt_idx = 0, vn_idx = 0;
                vertex_iss >> v_idx;
                if (vertex_iss >> vt_idx) {
                    vertex_iss >> vn_idx;
                }

                v_indices.push_back(v_idx - 1);
                vt_indices.push_back(vt_idx - 1);
                vn_indices.push_back(vn_idx - 1);
            }

            // Triangulate faces (assuming convex polygons)
            for (int i = 1; i < v_indices.size() - 1; i++) {
                indices.push_back(final_vertices.size());
                final_vertices.push_back(obj_vertices[v_indices[0]]);

                indices.push_back(final_vertices.size());
                final_vertices.push_back(obj_vertices[v_indices[i]]);

                indices.push_back(final_vertices.size());
                final_vertices.push_back(obj_vertices[v_indices[i + 1]]);
            }
        }
    }
    //hello 
    std::cout << "Loaded OBJ: " << obj_vertices.size()
        << " vertices, " << indices.size() / 3 << " triangles" << std::endl;
    std::cout << "Final vertices: " << final_vertices.size() << std::endl;

    return !final_vertices.empty();
}

const char* vertexShaderSource = R"(
#version 330 core
layout(location = 0) in vec3 aPos;
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
void main() {
    gl_Position = projection * view * model * vec4(aPos, 1.0);
}
)";

const char* fragmentShaderSource = R"(
#version 330 core
out vec4 FragColor;
void main() {
    FragColor = vec4(0.2, 0.8, 0.2, 1.0);
}
)";

unsigned int compileShader(unsigned int type, const char* src) {
    unsigned int shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, NULL);
    glCompileShader(shader);
    int success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char info[512];
        glGetShaderInfoLog(shader, 512, NULL, info);
        std::cerr << "Shader compile error: " << info << std::endl;
    }
    return shader;
}

unsigned int createShaderProgram() {
    unsigned int vs = compileShader(GL_VERTEX_SHADER, vertexShaderSource);
    unsigned int fs = compileShader(GL_FRAGMENT_SHADER, fragmentShaderSource);
    unsigned int prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);

    int success;
    glGetProgramiv(prog, GL_LINK_STATUS, &success);
    if (!success) {
        char info[512];
        glGetProgramInfoLog(prog, 512, NULL, info);
        std::cerr << "Shader program link error: " << info << std::endl;
    }

    glDeleteShader(vs);
    glDeleteShader(fs);
    return prog;
}

// Calculate bounding box for auto-centering and scaling
void calculateBounds(glm::vec3& min_bound, glm::vec3& max_bound, glm::vec3& center, float& scale) {
    if (final_vertices.empty()) return;

    min_bound = glm::vec3(final_vertices[0].x, final_vertices[0].y, final_vertices[0].z);
    max_bound = min_bound;

    for (const auto& v : final_vertices) {
        min_bound.x = std::min(min_bound.x, v.x);
        min_bound.y = std::min(min_bound.y, v.y);
        min_bound.z = std::min(min_bound.z, v.z);
        max_bound.x = std::max(max_bound.x, v.x);
        max_bound.y = std::max(max_bound.y, v.y);
        max_bound.z = std::max(max_bound.z, v.z);
        //hello
    }

    center = (min_bound + max_bound) * 0.5f;
    glm::vec3 size = max_bound - min_bound;
    scale = 2.0f / std::max({ size.x, size.y, size.z });

    std::cout << "Model bounds: min(" << min_bound.x << "," << min_bound.y << "," << min_bound.z << ") "
        << "max(" << max_bound.x << "," << max_bound.y << "," << max_bound.z << ")" << std::endl;
    std::cout << "Center: (" << center.x << "," << center.y << "," << center.z << ") Scale: " << scale << std::endl;
}

int main() {
    if (!glfwInit()) {
        std::cout << "Failed to init GLFW" << std::endl;
        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(800, 600, "OBJ Viewer", NULL, NULL);
    if (!window) {
        std::cout << "Failed to create window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);

    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) {
        std::cout << "Failed to init GLEW" << std::endl;
        return -1;
    }

    if (!loadOBJ("tinker.obj")) {
        std::cout << "Failed to load OBJ file or file is empty" << std::endl;
        return -1;
    }

    // Calculate auto-fit parameters
    glm::vec3 min_bound, max_bound, center;
    float auto_scale;
    calculateBounds(min_bound, max_bound, center, auto_scale);

    unsigned int VAO, VBO, EBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, final_vertices.size() * sizeof(Vertex), final_vertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
    glEnableVertexAttribArray(0);

    glBindVertexArray(0);

    unsigned int shader = createShaderProgram();

    float aspect = 800.0f / 600.0f;
    glm::mat4 projection = glm::perspective(glm::radians(45.0f), aspect, 0.1f, 100.0f);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    float rotation = 0.0f;

    while (!glfwWindowShouldClose(window)) {
        glClearColor(0.1f, 0.1f, 0.2f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(shader);

        rotation += 0.01f;

        // Model matrix: translate to center, scale, and rotate
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::rotate(model, rotation, glm::vec3(0.0f, 1.0f, 0.0f));
        model = glm::scale(model, glm::vec3(auto_scale));
        model = glm::translate(model, -center);

        // View matrix: camera positioned to look at the object
        glm::mat4 view = glm::lookAt(
            glm::vec3(0.0f, 0.0f, 4.0f),  // camera further back
            glm::vec3(0.0f, 0.0f, 0.0f),   // look at origin
            glm::vec3(0.0f, 1.0f, 0.0f)    // up vector
        );

        int modelLoc = glGetUniformLocation(shader, "model");
        int viewLoc = glGetUniformLocation(shader, "view");
        int projLoc = glGetUniformLocation(shader, "projection");

        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
        glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(projLoc, 1, GL_FALSE, glm::value_ptr(projection));

        glBindVertexArray(VAO);
        glDrawElements(GL_TRIANGLES, indices.size(), GL_UNSIGNED_INT, 0);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteBuffers(1, &EBO);
    glDeleteProgram(shader);

    glfwTerminate();
    return 0;
    //12345
}//1234
//123456