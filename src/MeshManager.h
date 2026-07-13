// #ifndef MESHMANAGER_H
// #define MESHMANAGER_H

// #include "Mesh.h"

// #include <assimp/Importer.hpp>
// #include <assimp/mesh.h>
// #include <fastgltf/tools.hpp>
// #include <fastgltf/types.hpp>
// #include <optional>
// #include <spdlog/spdlog.h>
// #include <unordered_map>
// #include <vector>

// namespace Expectre {

// class MeshManager {
// public:
//   static MeshManager &Instance();

//   std::vector<MeshHandle> consume_meshes_to_upload_to_gpu() {
//     return std::move(m_meshes_to_upload_to_gpu);
//   }

//   uint64_t compute_mesh_hash(const std::string &path, size_t gltf_index)
//   const;
//   // Delete copy constructor and assignment operator
//   MeshManager(const MeshManager &) = delete;
//   MeshManager &operator=(const MeshManager &) = delete;
//   MeshManager(MeshManager &&) = delete;
//   MeshManager &operator=(MeshManager &&) = delete;

//   const Mesh &get_mesh(MeshHandle mesh) {
//     auto it = m_mesh_map.find(mesh);
//     if (it != m_mesh_map.end()) {
//       return std::ref(it->second);
//     }
//     spdlog::warn("get_mesh(): Mesh {} not found, returning default mesh.",
//                  mesh.mesh_id);
//     return m_mesh_map[get_default_mesh()];
//   }

//   MeshHandle get_default_mesh();
//   void import_gltf_meshes(GltfCtx &ctx, flecs::world &world);

// private:
//   MeshManager() = default;
//   ~MeshManager() = default;

//   void compute_mesh_normals(Mesh &mesh);
//   void create_default_mesh();
//   MeshHandle import_mesh(const GltfCtx &ctx, const size_t gltf_mesh_index);

//   uint32_t m_next_mesh_id{0};
//   std::vector<MeshHandle> m_meshes_to_upload_to_gpu{};
//   std::unordered_map<MeshHandle, Mesh> m_mesh_map{};
//   MeshHandle m_default_mesh_handle{};
//   std::unordered_map<std::string, std::unordered_map<size_t, MeshHandle>>
//       gltf_mesh_map;
// };
// } // namespace Expectre
// #endif // MESHMANAGER_H