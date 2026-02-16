#ifndef MESHMANAGER_H
#define MESHMANAGER_H

#include "Mesh.h"

#include <assimp/Importer.hpp>
#include <assimp/mesh.h>
#include <vma/vk_mem_alloc.h>
#include <vulkan/vulkan.h>

#include <optional>
#include <unordered_map>
#include <vector>

namespace Expectre {

struct MeshAllocation {
  uint32_t vertex_offset;
  uint32_t vertex_count;
  uint32_t index_offset;
  uint32_t index_count;
};

struct GeometryBuffer {
  VmaAllocation allocation{VK_NULL_HANDLE}; // Allocation handle for the buffer
  VkBuffer buffer{}; // Handle to the Vulkan buffer object that the memory is
                     // bound to
  uint32_t vertex_offset = 0; // bytes
  uint32_t index_begin = 0;   // bytes
  uint32_t index_offset = 0;  // bytes
  uint32_t buffer_size = 0;   // Total size of the geometry buffer in bytes
  std::unordered_map<MeshHandle, MeshAllocation> mesh_allocations;
};

class MeshManager {
public:
  static MeshManager &Instance() {
    static MeshManager instance;
    return instance;
  }
  void init(VkDevice device, VmaAllocator allocator, VkCommandPool cmd_pool,
            VkQueue graphics_queue);
  MeshHandle import_mesh(aiMesh *ai_mesh);

  std::vector<MeshHandle> consume_meshes_to_upload_to_gpu() {
    return std::move(m_meshes_to_upload_to_gpu);
  }

  uint64_t compute_mesh_hash(const Mesh &mesh) const;
  // Delete copy constructor and assignment operator
  MeshManager(const MeshManager &) = delete;
  MeshManager &operator=(const MeshManager &) = delete;
  MeshManager(MeshManager &&) = delete;
  MeshManager &operator=(MeshManager &&) = delete;

  std::optional<std::reference_wrapper<const Mesh>> get_mesh(MeshHandle mesh) {
    auto it = m_mesh_map.find(mesh);
    if (it != m_mesh_map.end()) {
      return std::ref(it->second);
    }
    return std::nullopt;
  }

private:
  MeshManager() = default;
  ~MeshManager() = default;

  void compute_mesh_normals(Mesh &mesh);

  uint32_t m_next_mesh_id{0};
  std::vector<MeshHandle> m_meshes_to_upload_to_gpu{};
  std::unordered_map<MeshHandle, Mesh> m_mesh_map{};
  void upload_mesh_to_gpu(const Mesh &mesh, VkDevice device,
                          VmaAllocator allocator, VkCommandPool cmd_pool,
                          VkQueue graphics_queue);
  GeometryBuffer m_geometry_buffer{};
  std::vector<MeshAllocation> m_mesh_allocations{};
  VmaAllocator m_allocator;
  VkDevice m_device;
  VkCommandPool m_cmd_pool;
  VkQueue m_graphics_queue;
};
} // namespace Expectre
#endif // MESHMANAGER_H