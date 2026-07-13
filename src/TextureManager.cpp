// #include "TextureManager.h"
// #include <spdlog/spdlog.h>
// // #define STB_IMAGE_IMPLEMENTATION // includes stb function bodies
// #include <glm/gtc/type_ptr.hpp>
// #include <stb_image.h>
// #include <variant>
// #include <xxhash.h>

// namespace Expectre {

// TextureManager::TextureManager() {}

// void TextureManager::import_gltf_images(GltfCtx &ctx) {

//   ctx.texture_handles.resize(ctx.asset.images.size());
//   for (size_t image_index = 0; image_index < ctx.asset.images.size();
//        ++image_index) {

//     const auto &gltf_img = ctx.asset.images[image_index];
//     const TextureHandle tex_handle = import_gltf_image(ctx, image_index);
//     ctx.texture_handles[image_index] = tex_handle;
//   }
// }

// Material TextureManager::import_material(const GltfCtx &ctx,
//                                          const size_t material_index) {

//   // const auto &gltf_material = ctx.asset.materials[material_index];
//   // Material current_material{};
//   // current_material.albedo_factor =
//   //     glm::make_vec4(gltf_material.pbrData.baseColorFactor.data());
//   // current_material.metallic_factor = gltf_material.pbrData.metallicFactor;
//   // current_material.roughness_factor =
//   gltf_material.pbrData.roughnessFactor;

//   // if (gltf_material.pbrData.baseColorTexture.has_value()) {

//   // } else {
//   // }

//   // if (gltf_material.pbrData.metallicRoughnessTexture.has_value()) {

//   // } else {
//   // }
//   // const auto &gltf_albedo =
//   //
//   ctx.asset.textures[gltf_material.pbrData.baseColorTexture->textureIndex];
//   // const auto &gltf_metallic =
//   //     ctx.asset.textures[gltf_material.pbrData.metallicRoughnessTexture
//   //                            ->textureIndex];
//   // const auto &gltf_roughness =
//   //     ctx.asset.textures[gltf_material.pbrData.metallicRoughnessTexture
//   //                            ->textureIndex];
//   // current_material.albedo =
//   //     import_texture("albedo_" + gltf_material.name) return {};
// }

// void TextureManager::create_default_texture() {
//   if (m_default_texture_handle) {
//     return;
//   }
//   constexpr uint32_t texture_size = 64;
//   // size of checker blocks
//   constexpr uint32_t block_size = 8;
//   constexpr uint32_t channels = 4; // RGBA

//   uint8_t *data =
//       static_cast<uint8_t *>(malloc(texture_size * texture_size * channels));

//   const uint8_t magenta[] = {255, 0, 255, 255};
//   const uint8_t black[] = {0, 0, 0, 255};

//   for (uint32_t y = 0; y < texture_size; ++y) {
//     for (uint32_t x = 0; x < texture_size; ++x) {
//       const bool use_magenta = (((x / block_size) + (y / block_size)) % 2) ==
//       0;

//       const uint8_t *color = use_magenta ? magenta : black;

//       uint32_t pixel_index = (y * texture_size + x) * channels;

//       data[pixel_index + 0] = color[0];
//       data[pixel_index + 1] = color[1];
//       data[pixel_index + 2] = color[2];
//       data[pixel_index + 3] = color[3];
//     }
//   }

//   m_default_texture_handle =
//       store_texture("Tex/__default_checkerboard__", data, texture_size,
//                     texture_size, channels);
// }

// TextureHandle TextureManager::get_default_texture() {
//   if (!m_default_texture_handle) {
//     create_default_texture();
//   }
//   return m_default_texture_handle;
// }

// uint64_t TextureManager::compute_texture_hash(const std::string &path,
//                                               const size_t gltf_index) const
//                                               {
//   // 1. Instantly hash the path string on the stack (Seed = 0)
//   uint64_t path_hash = XXH64(path.data(), path.size(), 0);

//   // 2. Hash the raw binary bytes of the index on the stack,
//   // using the path_hash as the unique seed to combine them perfectly!
//   return XXH64(&gltf_index, sizeof(gltf_index), path_hash);
// }

// TextureHandle
// TextureManager::import_texture_from_file(const std::filesystem::path &path) {
//   int width = 0;
//   int height = 0;
//   int original_channels = 0;

//   constexpr int desired_channels = 4;

//   stbi_uc *pixels = stbi_load(path.string().c_str(), &width, &height,
//                               &original_channels, desired_channels);

//   if (!pixels) {
//     spdlog::error("Failed to load texture '{}': {}", path.string(),
//                   stbi_failure_reason());
//     return get_default_texture();
//   }

//   const std::string name = path.generic_string();

//   return store_texture(name, pixels, static_cast<uint32_t>(width),
//                        static_cast<uint32_t>(height), 4);
// }

// TextureHandle TextureManager::import_texture_from_memory(std::string name,
//                                                          const uint8_t
//                                                          *bytes, size_t
//                                                          byte_count) {
//   if (!bytes || byte_count == 0) {
//     spdlog::error("Cannot load texture '{}': image bytes are empty", name);
//     return get_default_texture();
//   }

//   int width = 0;
//   int height = 0;
//   int original_channels = 0;

//   stbi_uc *pixels =
//       stbi_load_from_memory(bytes, static_cast<int>(byte_count), &width,
//                             &height, &original_channels, STBI_rgb_alpha);

//   if (!pixels) {
//     spdlog::error("Failed to decode texture '{}': {}", name,
//                   stbi_failure_reason());
//     return get_default_texture();
//   }

//   return store_texture(std::move(name), pixels, static_cast<uint32_t>(width),
//                        static_cast<uint32_t>(height), 4);
// }

// TextureHandle TextureManager::store_texture(std::string name, uint8_t
// *pixels,
//                                             uint32_t width, uint32_t height,
//                                             uint8_t channels) {
//   if (!pixels) {
//     spdlog::error("Cannot store texture '{}': pixels are null", name);
//     return get_default_texture();
//   }

//   compute_texture_hash()

//       TextureHandle handle{};
//   handle.texture_id = static_cast<int64_t>(hash);

//   if (m_texture_map.find(handle) != m_texture_map.end()) {
//     spdlog::warn("Texture '{}' already cached. Reusing handle {}", name,
//     hash); stbi_image_free(pixels); return handle;
//   }

//   Texture texture{};
//   texture.name = std::move(name);
//   texture.data = pixels;
//   texture.width = width;
//   texture.height = height;
//   texture.channels = channels;

//   m_texture_map.emplace(handle, std::move(texture));

//   // texture will be later uploaded to GPU
//   m_textures_to_upload_to_gpu.push_back(handle);

//   return handle;
// }

// TextureHandle TextureManager::import_gltf_image(const GltfCtx &ctx,
//                                                 size_t image_index) {
//   const auto &image = ctx.asset.images[image_index];
//   const std::string qualified_name = make_qualified_gltf_name(ctx,
//   image_index);

//   const uint64_t hash =
//       compute_texture_hash(ctx.path.generic_string(), image_index);

//   TextureHandle handle{};
//   handle.texture_id = static_cast<int64_t>(hash);

//   if (m_texture_map.find(handle) != m_texture_map.end()) {
//     // image already imported!
//     return handle;
//   }

//   TextureHandle result = get_default_texture();

//   std::visit(
//       fastgltf::visitor{
//           [&](const std::monostate &) {
//             spdlog::error("glTF image {} has no data source", image_index);
//             result = get_default_texture();
//           },

//           [&](const fastgltf::sources::URI &source) {
//             if (!source.uri.isLocalPath()) {
//               spdlog::error("glTF image {} uses non-local URI", image_index);
//               result = get_default_texture();
//               return;
//             }

//             if (source.fileByteOffset != 0) {
//               spdlog::error(
//                   "glTF image {} has fileByteOffset != 0, unsupported",
//                   image_index);
//               result = get_default_texture();
//               return;
//             }

//             const auto full_path = ctx.path.parent_path() /
//             source.uri.path();

//             // This uses the file path as the hash unless you add an overload
//             // that accepts the glTF image hash.
//             result = import_texture_from_file(full_path);
//           },

//           [&](const fastgltf::sources::Array &source) {
//             result = import_texture_from_memory(
//                 name, reinterpret_cast<const uint8_t *>(source.bytes.data()),
//                 source.bytes.size());
//           },

//           [&](const fastgltf::sources::BufferView &source) {
//             const auto &buffer_view =
//                 ctx.asset.bufferViews[source.bufferViewIndex];

//             const auto &buffer = ctx.asset.buffers[buffer_view.bufferIndex];

//             std::visit(
//                 fastgltf::visitor{
//                     [&](const fastgltf::sources::Array &buffer_source) {
//                       const uint8_t *start = reinterpret_cast<const uint8_t
//                       *>(
//                           buffer_source.bytes.data() +
//                           buffer_view.byteOffset);

//                       result = import_texture_from_memory(
//                           name, start, buffer_view.byteLength, hash);
//                     },

//                     [&](const auto &) {
//                       spdlog::error(
//                           "Unsupported buffer source for glTF image {}",
//                           image_index);
//                       result = get_default_texture();
//                     }},
//                 buffer.data);
//           },

//           [&](const auto &) {
//             spdlog::error("Unsupported source for glTF image {}",
//             image_index); result = get_default_texture();
//           }},
//       image.data);

//   return result;
// }
// } // namespace Expectre