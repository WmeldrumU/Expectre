#include "MeshManager.h"
#include "ToolsVk.h"

#include <spdlog/spdlog.h>
#include <vulkan/vulkan.h>
#include <xxhash.h>
namespace Expectre {

MeshManager &MeshManager::Instance() {
  static MeshManager instance;
  return instance;
}

uint64_t MeshManager::compute_mesh_hash(const Mesh &mesh) const {
  XXH64_state_t *state = XXH64_createState();
  XXH64_reset(state, 0);

  // Hash vertex data
  XXH64_update(state, mesh.vertices.data(),
               mesh.vertices.size() * sizeof(Vertex));

  // Hash index data
  XXH64_update(state, mesh.indices.data(),
               mesh.indices.size() * sizeof(uint32_t));

  uint64_t hash = XXH64_digest(state);
  XXH64_freeState(state);

  return hash;
}

void MeshManager::compute_mesh_normals(Mesh &mesh) {

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

MeshHandle MeshManager::import_mesh(aiMesh *ai_mesh) {
  Mesh mesh{};
  mesh.name = ai_mesh->mName.C_Str();
  mesh.vertices.reserve(ai_mesh->mNumVertices);
  mesh.indices.reserve(ai_mesh->mNumFaces * 3);

  // Iterate through each vertex of the mesh
  for (unsigned int j = 0; j < ai_mesh->mNumVertices; j++) {
    Expectre::Vertex vertex;

    // Positions
    vertex.pos.x = ai_mesh->mVertices[j].x;
    vertex.pos.y = ai_mesh->mVertices[j].y;
    vertex.pos.z = ai_mesh->mVertices[j].z;

    // Normals
    if (ai_mesh->HasNormals()) {
      vertex.normal.x = ai_mesh->mNormals[j].x;
      vertex.normal.y = ai_mesh->mNormals[j].y;
      vertex.normal.z = ai_mesh->mNormals[j].z;
    } else {
      // Will calculate normals after processing all vertices
      vertex.normal = glm::vec3(0.0f);
    }

    // Texture Coordinates
    if (ai_mesh->mTextureCoords[0]) { // Check if the mesh contains texture
                                      // coordinates
      vertex.tex_coord = {ai_mesh->mTextureCoords[0][j].x,
                          ai_mesh->mTextureCoords[0][j].y};
    } else {
      vertex.tex_coord = glm::vec2(0.0f, 0.0f);
    }

    // set vertex color to black for now
    vertex.color = glm::vec3(0.0f);

    mesh.vertices.push_back(vertex);
  }

  // Indices
  for (auto j = 0; j < ai_mesh->mNumFaces; j++) {
    aiFace &face = ai_mesh->mFaces[j];
    for (auto k = 0; k < face.mNumIndices; k++) {
      mesh.indices.push_back(face.mIndices[k]);
    }
  }

  // Calculate normals if the mesh doesn't have them
  if (!ai_mesh->HasNormals()) {
    compute_mesh_normals(mesh);
  }

  // Compute hash and check for duplicates
  uint64_t hash = compute_mesh_hash(mesh);
  MeshHandle handle{};
  handle.mesh_id = hash;
  if (m_mesh_map.find(handle) != m_mesh_map.end()) {
    // Mesh already exists, return existing ID
    return handle;
  }

  // New unique mesh
  m_mesh_map[handle] = std::move(mesh);
  m_meshes_to_upload_to_gpu.push_back(handle);

  return handle;
}

} // namespace Expectre