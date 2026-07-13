#ifndef MESH_H
#define MESH_H

#include <assimp/Importer.hpp>
#include <assimp/defs.h>
#include <assimp/mesh.h>
#include <flecs.h>
#include <glm/glm.hpp>
#include <string>
#include <vector>

namespace Expectre {

struct Vertex {
  glm::vec3 pos = glm::vec3(0.0f);
  glm::vec3 color = glm::vec3(1.0f); // Default white vertex color
  glm::vec3 normal = glm::vec3(0.0f);
  glm::vec2 tex_coord = glm::vec2(0.0f);
};

// ECS
struct Node {};
struct UsesMesh {};

// sub-mesh that has one material
// gltf describes as "one draw call"
struct Primitive {
  uint32_t index_count = 0;
  uint32_t first_index = 0;
  int32_t vertex_offset = 0;
  flecs::entity material;
};

struct PendingPrimitiveUpload {
  std::vector<Vertex> vertices;
  std::vector<uint32_t> indices;
};

struct Mesh {
  std::string name; // Name of the mesh
};

void compute_vertex_normals(PendingPrimitiveUpload &mesh) {

  // Initialize all normals to zero
  for (auto &vertex : mesh.vertices) {
    vertex.normal = glm::vec3(0.0f);
  }

  // Calculate face normals and accumulate to vertex normals
  for (size_t i = 0; i < mesh.indices.size(); i += 3) {
    uint32_t idx0 = mesh.indices[i];
    uint32_t idx1 = mesh.indices[i + 1];
    uint32_t idx2 = mesh.indices[i + 2];

    glm::vec3 v0 = mesh.vertices[idx0].pos;
    glm::vec3 v1 = mesh.vertices[idx1].pos;
    glm::vec3 v2 = mesh.vertices[idx2].pos;

    // Calculate face normal using cross product
    glm::vec3 edge1 = v1 - v0;
    glm::vec3 edge2 = v2 - v0;
    glm::vec3 face_normal = glm::cross(edge1, edge2);

    // Accumulate face normal to all three vertices
    mesh.vertices[idx0].normal += face_normal;
    mesh.vertices[idx1].normal += face_normal;
    mesh.vertices[idx2].normal += face_normal;
  }

  // Normalize all vertex normals
  for (auto &vertex : mesh.vertices) {
    if (glm::length(vertex.normal) > 0.0f) {
      vertex.normal = glm::normalize(vertex.normal);
    } else {
      // Fallback for degenerate vertices
      vertex.normal = glm::vec3(0.0f, 1.0f, 0.0f);
    }
  }
}

} // namespace Expectre

#endif // MESH_H