// #include "MeshManager.h"
// #include "ToolsVk.h"
// #include "scene/AssetImporter.h"

// #include <fastgltf/glm_element_traits.hpp>
// #include <spdlog/spdlog.h>
// #include <vulkan/vulkan.h>

// #include <xxhash.h>
// namespace Expectre {

// MeshManager &MeshManager::Instance() {
//   static MeshManager instance;
//   return instance;
// }

// uint64_t MeshManager::compute_mesh_hash(const std::string &path,
//                                         const size_t gltf_index) const {
//   // 1. Instantly hash the path string on the stack (Seed = 0)
//   uint64_t path_hash = XXH64(path.data(), path.size(), 0);

//   // 2. Hash the raw binary bytes of the index on the stack,
//   // using the path_hash as the unique seed to combine them perfectly!
//   return XXH64(&gltf_index, sizeof(gltf_index), path_hash);
// }
// void MeshManager::compute_mesh_normals(Mesh &mesh) {

//   // Initialize all normals to zero
//   for (auto &vertex : mesh.vertices) {
//     vertex.normal = glm::vec3(0.0f);
//   }

//   // Calculate face normals and accumulate to vertex normals
//   for (size_t i = 0; i < mesh.indices.size(); i += 3) {
//     uint32_t idx0 = mesh.indices[i];
//     uint32_t idx1 = mesh.indices[i + 1];
//     uint32_t idx2 = mesh.indices[i + 2];

//     glm::vec3 v0 = mesh.vertices[idx0].pos;
//     glm::vec3 v1 = mesh.vertices[idx1].pos;
//     glm::vec3 v2 = mesh.vertices[idx2].pos;

//     // Calculate face normal using cross product
//     glm::vec3 edge1 = v1 - v0;
//     glm::vec3 edge2 = v2 - v0;
//     glm::vec3 face_normal = glm::cross(edge1, edge2);

//     // Accumulate face normal to all three vertices
//     mesh.vertices[idx0].normal += face_normal;
//     mesh.vertices[idx1].normal += face_normal;
//     mesh.vertices[idx2].normal += face_normal;
//   }

//   // Normalize all vertex normals
//   for (auto &vertex : mesh.vertices) {
//     if (glm::length(vertex.normal) > 0.0f) {
//       vertex.normal = glm::normalize(vertex.normal);
//     } else {
//       // Fallback for degenerate vertices
//       vertex.normal = glm::vec3(0.0f, 1.0f, 0.0f);
//     }
//   }
// }

// void MeshManager::create_default_mesh() {
//   // Unit cube centered at origin with per-face UVs and outward normals.
//   // Each face has 4 unique vertices (no sharing) so UVs and normals are
//   // correct.
//   Mesh mesh{};
//   mesh.name = "__default_cube__";

//   // Helper: { pos, color, normal, uv }
//   // +X face (right)
//   mesh.vertices.insert(
//       mesh.vertices.end(),
//       {
//           {{0.5f, -0.5f, -0.5f}, {1, 1, 1}, {1, 0, 0}, {0, 1}},
//           {{0.5f, 0.5f, -0.5f}, {1, 1, 1}, {1, 0, 0}, {0, 0}},
//           {{0.5f, 0.5f, 0.5f}, {1, 1, 1}, {1, 0, 0}, {1, 0}},
//           {{0.5f, -0.5f, 0.5f}, {1, 1, 1}, {1, 0, 0}, {1, 1}},
//           // -X face (left)
//           {{-0.5f, -0.5f, 0.5f}, {1, 1, 1}, {-1, 0, 0}, {0, 1}},
//           {{-0.5f, 0.5f, 0.5f}, {1, 1, 1}, {-1, 0, 0}, {0, 0}},
//           {{-0.5f, 0.5f, -0.5f}, {1, 1, 1}, {-1, 0, 0}, {1, 0}},
//           {{-0.5f, -0.5f, -0.5f}, {1, 1, 1}, {-1, 0, 0}, {1, 1}},
//           // +Y face (top)
//           {{-0.5f, 0.5f, -0.5f}, {1, 1, 1}, {0, 1, 0}, {0, 1}},
//           {{-0.5f, 0.5f, 0.5f}, {1, 1, 1}, {0, 1, 0}, {0, 0}},
//           {{0.5f, 0.5f, 0.5f}, {1, 1, 1}, {0, 1, 0}, {1, 0}},
//           {{0.5f, 0.5f, -0.5f}, {1, 1, 1}, {0, 1, 0}, {1, 1}},
//           // -Y face (bottom)
//           {{-0.5f, -0.5f, 0.5f}, {1, 1, 1}, {0, -1, 0}, {0, 1}},
//           {{-0.5f, -0.5f, -0.5f}, {1, 1, 1}, {0, -1, 0}, {0, 0}},
//           {{0.5f, -0.5f, -0.5f}, {1, 1, 1}, {0, -1, 0}, {1, 0}},
//           {{0.5f, -0.5f, 0.5f}, {1, 1, 1}, {0, -1, 0}, {1, 1}},
//           // +Z face (front)
//           {{-0.5f, -0.5f, 0.5f}, {1, 1, 1}, {0, 0, 1}, {0, 1}},
//           {{0.5f, -0.5f, 0.5f}, {1, 1, 1}, {0, 0, 1}, {1, 1}},
//           {{0.5f, 0.5f, 0.5f}, {1, 1, 1}, {0, 0, 1}, {1, 0}},
//           {{-0.5f, 0.5f, 0.5f}, {1, 1, 1}, {0, 0, 1}, {0, 0}},
//           // -Z face (back)
//           {{0.5f, -0.5f, -0.5f}, {1, 1, 1}, {0, 0, -1}, {0, 1}},
//           {{-0.5f, -0.5f, -0.5f}, {1, 1, 1}, {0, 0, -1}, {1, 1}},
//           {{-0.5f, 0.5f, -0.5f}, {1, 1, 1}, {0, 0, -1}, {1, 0}},
//           {{0.5f, 0.5f, -0.5f}, {1, 1, 1}, {0, 0, -1}, {0, 0}},
//       });

//   // Two triangles per face, 6 faces
//   for (uint32_t face = 0; face < 6; ++face) {
//     uint32_t base = face * 4;
//     mesh.indices.insert(mesh.indices.end(), {
//                                                 base,
//                                                 base + 1,
//                                                 base + 2,
//                                                 base,
//                                                 base + 2,
//                                                 base + 3,
//                                             });
//   }

//   uint64_t hash = compute_mesh_hash("EXPECTRE_DEFAULT_MESH", 0);
//   m_default_mesh_handle.mesh_id = hash;

//   m_mesh_map[m_default_mesh_handle] = std::move(mesh);
//   m_meshes_to_upload_to_gpu.push_back(m_default_mesh_handle);
// }

// MeshHandle MeshManager::get_default_mesh() {
//   if (!m_default_mesh_handle) {
//     create_default_mesh();
//   }
//   return m_default_mesh_handle;
// }

// void MeshManager::import_gltf_meshes(GltfCtx &ctx, flecs::world &world) {

//   ctx.mesh_ents.resize(ctx.asset.meshes.size());

//   // For each mesh, import mesh and create entity from the returned handle
//   for (size_t mesh_index = 0; mesh_index < ctx.asset.meshes.size();
//        ++mesh_index) {
//     const auto &gltf_mesh = ctx.asset.meshes[mesh_index];

//     const MeshHandle mesh_handle =
//         MeshManager::Instance().import_mesh(ctx, mesh_index);
//     const std::string mesh_name =
//         MeshManager::Instance().get_mesh(mesh_handle).name;

//     flecs::entity mesh_ent =
//         world.entity(mesh_name.c_str()).set<MeshHandle>({mesh_handle});

//     ctx.mesh_ents[mesh_index] = mesh_ent;
//   }
// }

// MeshHandle MeshManager::import_mesh(const GltfCtx &ctx,
//                                     const size_t gltf_mesh_index) {

//   const fastgltf::Mesh &gltf_mesh = ctx.asset.meshes[gltf_mesh_index];

//   Mesh out_mesh{};
//   out_mesh.name =
//       "Mesh/" + std::string(gltf_mesh.name.empty()
//                                 ? ctx.path.generic_string() + "_" +
//                                       (std::to_string(gltf_mesh_index)).c_str()
//                                 : gltf_mesh.name.c_str());

//   // Compute hash and check for duplicates
//   const uint64_t hash = compute_mesh_hash(out_mesh.name, gltf_mesh_index);
//   MeshHandle handle{};
//   handle.mesh_id = hash;

//   if (m_mesh_map.find(handle) != m_mesh_map.end()) {
//     // Mesh already exists, return existing ID
//     return handle;
//   }

//   for (const fastgltf::Primitive &prim : gltf_mesh.primitives) {
//     if (prim.type != fastgltf::PrimitiveType::Triangles) {
//       continue;
//     }

//     // Read indices
//     // indices are guaranteed through fastgltf::Options::GenerateMeshIndices
//     const auto &index_accessor =
//         ctx.asset.accessors[prim.indicesAccessor.value()];
//     out_mesh.indices.resize(index_accessor.count);

//     fastgltf::iterateAccessor<uint32_t>(
//         ctx.asset, index_accessor,
//         [&](uint32_t index) { out_mesh.indices.push_back(index); });

//     // Position
//     const auto *pos_attr = prim.findAttribute("POSITION");
//     if (pos_attr != prim.attributes.end()) {
//       const fastgltf::Accessor &positions_accessor =
//           ctx.asset.accessors[pos_attr->accessorIndex];
//       out_mesh.vertices.resize(positions_accessor.count);

//       fastgltf::iterateAccessorWithIndex<glm::vec3>(
//           ctx.asset, positions_accessor,
//           [&](glm::vec3 pos, size_t idx) { out_mesh.vertices[idx].pos = pos;
//           }

//       );
//     }

//     // Normals
//     const auto *norm_attr = prim.findAttribute("NORMAL");
//     if (norm_attr != prim.attributes.end()) {
//       const auto &norm_accessor =
//       ctx.asset.accessors[norm_attr->accessorIndex];
//       fastgltf::iterateAccessorWithIndex<glm::vec3>(
//           ctx.asset, norm_accessor,
//           [&](glm::vec3 normal, size_t idx) {
//             out_mesh.vertices[idx].normal = normal;
//           }

//       );
//     } else {
//       compute_mesh_normals(out_mesh);
//     }

//     // UV Coords
//     const auto *uv_attr = prim.findAttribute("TEXCOORD_0");
//     if (uv_attr != prim.attributes.end()) {
//       const auto &uv_accessor = ctx.asset.accessors[uv_attr->accessorIndex];

//       fastgltf::iterateAccessorWithIndex<glm::vec2>(
//           ctx.asset, uv_accessor, [&](glm::vec2 uv, size_t idx) {
//             out_mesh.vertices[idx].tex_coord = uv;
//           });
//     }

//     // Vertex color
//     const auto *vert_color_attr = prim.findAttribute("COLOR_0");
//     if (vert_color_attr != prim.attributes.end()) {
//       const auto &vert_color_accessor =
//           ctx.asset.accessors[vert_color_attr->accessorIndex];

//       fastgltf::iterateAccessorWithIndex<glm::vec3>(
//           ctx.asset, vert_color_accessor, [&](glm::vec3 color, size_t idx) {
//             out_mesh.vertices[idx].color = color;
//           });
//     }
//   }

//   // Register mesh
//   m_mesh_map[handle] = std::move(out_mesh);
//   m_meshes_to_upload_to_gpu.push_back(handle);

//   return handle;
// }

// } // namespace Expectre