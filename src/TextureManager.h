// #ifndef TEXTURE_MANAGER_H
// #define TEXTURE_MANAGER_H

// #include "Material.h"
// #include "Texture.h"

// #include <filesystem>
// #include <string>
// #include <unordered_map>
// #include <vector>

// #include <fastgltf/tools.hpp>
// #include <fastgltf/types.hpp>
// #include <glm/gtc/type_ptr.hpp>
// #include <spdlog/spdlog.h>
// #include <stb_image.h>
// #include <type_traits>
// namespace Expectre {
// class TextureManager {
// public:
//   static TextureManager &Instance() {
//     static TextureManager instance;
//     return instance;
//   }

//   void import_gltf_images(GltfCtx &ctx);
//   const Texture &get_texture(TextureHandle texture) {
//     auto it = m_texture_map.find(texture);
//     if (it != m_texture_map.end()) {
//       return it->second;
//     }
//     // Texture not found, return default checkerboard
//     spdlog::warn("Texture handle {} not found, using default texture",
//                  texture.texture_id);
//     return m_texture_map[m_default_texture_handle];
//   }
//   std::vector<TextureHandle> consume_textures_to_upload_to_gpu() {
//     return std::move(m_textures_to_upload_to_gpu);
//   }

//   // Delete copy constructor and assignment operator
//   TextureManager(const TextureManager &) = delete;
//   TextureManager &operator=(const TextureManager &) = delete;
//   TextureManager(TextureManager &&) = delete;
//   TextureManager &operator=(TextureManager &&) = delete;

//   TextureHandle get_default_texture();
//   Material get_default_material() {
//     TextureHandle _default = get_default_texture();
//     return Material{"___EXP_DEFAULT_MATERIAL____", _default, _default,
//     _default,
//                     _default};
//   }
//   Material import_material(const GltfCtx &ctx, const size_t material_index);

// private:
//   TextureManager();
//   ~TextureManager() = default;

//   void create_default_texture();

//   std::string make_qualified_gltf_name(const GltfCtx &ctx,
//                                        size_t image_index) const;
//   TextureHandle import_gltf_image(const GltfCtx &ctx, const size_t
//   image_index);

//   TextureHandle import_texture_from_memory(std::string name,
//                                            const uint8_t *bytes,
//                                            size_t byte_count);
//   TextureHandle import_texture_from_file(const std::filesystem::path &path);

//   TextureHandle store_texture(std::string name, uint8_t *pixels, uint32_t
//   width,
//                               uint32_t height, uint8_t channels);
//   uint64_t compute_texture_hash(const std::string &path,
//                                 const size_t gltf_index) const;

//   std::vector<TextureHandle> m_textures_to_upload_to_gpu{};
//   std::unordered_map<TextureHandle, Texture> m_texture_map{};
//   TextureHandle m_default_texture_handle{};
// };
// } // namespace Expectre

// #endif // TEXTURE_MANAGER_H