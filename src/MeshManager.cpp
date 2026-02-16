#include "MeshManager.h"
#include "ToolsVk.h"

#include <spdlog/spdlog.h>
#include <vulkan/vulkan.h>
#include <xxhash.h>
namespace Expectre {

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

void MeshManager::upload_mesh_to_gpu(const Mesh &mesh, VkDevice device,
                                     VmaAllocator allocator,
                                     VkCommandPool cmd_pool,
                                     VkQueue graphics_queue) {
  // Early return if mesh is empty
  if (mesh.vertices.empty() || mesh.indices.empty()) {
    spdlog::warn("Attempted to upload empty mesh to GPU");
    return;
  }

  VkDeviceSize vertex_buffer_size = sizeof(Vertex) * mesh.vertices.size();
  VkDeviceSize index_buffer_size = sizeof(uint32_t) * mesh.indices.size();

  // Align vertex buffer size to 4 bytes to keep index offsets aligned
  vertex_buffer_size = (vertex_buffer_size + 3) & ~3;
  // Align vertex buffer size to 4 bytes to keep index offsets aligned
  index_buffer_size = (index_buffer_size + 3) & ~3;

  VkDeviceSize mesh_buffer_size = vertex_buffer_size + index_buffer_size;

  // === STAGING BUFFERS ===
  AllocatedBuffer mesh_staging = ToolsVk::create_buffer(
      allocator, mesh_buffer_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
      VMA_MEMORY_USAGE_CPU_ONLY,
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
          VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

  void *data{nullptr};
  VK_CHECK_RESULT(vmaMapMemory(allocator, mesh_staging.allocation, &data));
  uint8_t *start_ptr = static_cast<uint8_t *>(data);
  memcpy(start_ptr, mesh.vertices.data(), vertex_buffer_size);
  memcpy(start_ptr + vertex_buffer_size, mesh.indices.data(),
         index_buffer_size);

  // === copy from staging to device buffer

  VkBufferCopy vertex_region{};
  vertex_region.srcOffset = 0;
  vertex_region.dstOffset =
      static_cast<VkDeviceSize>(m_geometry_buffer.vertex_offset);
  vertex_region.size = vertex_buffer_size;

  // Verify this won't write over our index buffer
  assert(m_geometry_buffer.vertex_offset + vertex_buffer_size <=
         m_geometry_buffer.index_begin);

  // first, copy vertices
  ToolsVk::copy_buffer(device, cmd_pool, graphics_queue, mesh_staging.buffer,
                       m_geometry_buffer.buffer, vertex_region);
  m_geometry_buffer.vertex_offset += vertex_buffer_size;

  VkBufferCopy index_region{};
  index_region.srcOffset = static_cast<VkDeviceSize>(vertex_buffer_size);
  index_region.dstOffset =
      static_cast<VkDeviceSize>(m_geometry_buffer.index_offset);
  index_region.size = index_buffer_size;

  // Verify this won't write outside the geometry buffer
  assert(m_geometry_buffer.index_offset + index_buffer_size <=
         m_geometry_buffer.buffer_size);

  // second, copy indices
  ToolsVk::copy_buffer(device, cmd_pool, graphics_queue, mesh_staging.buffer,
                       m_geometry_buffer.buffer, index_region);
  m_geometry_buffer.index_offset += index_buffer_size;

  // Create an entry to keep track of meshes in our geometry buffer
  MeshAllocation alloc{};
  alloc.index_count = mesh.indices.size();
  alloc.vertex_count = mesh.vertices.size();

  // Calculate where this mesh starts in the buffer (before we incremented the
  // offsets)
  uint32_t mesh_index_start_bytes =
      m_geometry_buffer.index_offset - index_buffer_size;
  uint32_t mesh_vertex_start_bytes =
      m_geometry_buffer.vertex_offset - vertex_buffer_size;

  // Convert to position relative to the start of each section
  uint32_t index_section_offset_bytes =
      mesh_index_start_bytes - m_geometry_buffer.index_begin;
  uint32_t vertex_section_offset_bytes =
      mesh_vertex_start_bytes; // Vertices start at byte 0

  // Convert from byte offsets to element counts (what vkCmdDrawIndexed expects)
  alloc.index_offset = index_section_offset_bytes / sizeof(uint32_t);
  alloc.vertex_offset = vertex_section_offset_bytes / sizeof(Vertex);

  m_mesh_allocations.push_back(alloc);

  // Cleanup staging
  vmaUnmapMemory(m_allocator, mesh_staging.allocation);
  vmaDestroyBuffer(m_allocator, mesh_staging.buffer, mesh_staging.allocation);
}

} // namespace Expectre