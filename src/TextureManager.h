#ifndef TEXTURE_MANAGER_H
#define TEXTURE_MANAGER_H

#include "Material.h"
#include "Texture.h"

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

#include <assimp/material.h>
#include <assimp/scene.h>
#include <fastgltf/tools.hpp>
#include <fastgltf/types.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <spdlog/spdlog.h>
#include <stb_image.h>
#include <type_traits>
namespace Expectre {
class TextureManager {
public:
  static TextureManager &Instance() {
    static TextureManager instance;
    return instance;
  }

  TextureHandle import_texture(std::string image_directory);
  // TextureHandle TextureManager::import_texture(void *data);
  TextureHandle import_texture(std::string name, void *data, uint32_t width,
                               uint32_t height, uint32_t channels);

  std::vector<TextureHandle> consume_textures_to_upload_to_gpu() {
    return std::move(m_textures_to_upload_to_gpu);
  }

  // Load texture from file - populates CPU data only, no GPU resources
  // Caller is responsible for calling
  // RenderResourceManager::upload_texture_to_gpu
  void load_texture_from_file(Texture &texture, const std::string &filepath);

  uint64_t compute_texture_hash(const Texture &texture) const;

  // Delete copy constructor and assignment operator
  TextureManager(const TextureManager &) = delete;
  TextureManager &operator=(const TextureManager &) = delete;
  TextureManager(TextureManager &&) = delete;
  TextureManager &operator=(TextureManager &&) = delete;

  const Texture &get_texture(TextureHandle texture) {
    auto it = m_texture_map.find(texture);
    if (it != m_texture_map.end()) {
      return it->second;
    }
    // Texture not found, return default checkerboard
    spdlog::warn("Texture handle {} not found, using default texture",
                 texture.texture_id);
    return m_texture_map[m_default_texture_handle];
  }

  TextureHandle get_default_texture();
  Material get_default_material() {
    TextureHandle _default = get_default_texture();
    return Material{"___EXP_DEFAULT_MATERIAL____", _default, _default, _default,
                    _default};
  }

  // Load a texture from an assimp material, handling both file-based and
  // GLB-embedded textures (paths starting with '*').
  TextureHandle load_texture_from_material(const aiScene *scene,
                                           const aiMaterial *ai_material,
                                           aiTextureType texture_type,
                                           const std::string &model_directory) {
    auto count = ai_material->GetTextureCount(texture_type);
    spdlog::debug("[TEXLOAD] type={} count={} dir='{}'", (int)texture_type,
                  count, model_directory);
    if (count == 0) {
      return {};
    }

    aiString texture_path;
    ai_material->GetTexture(texture_type, 0, &texture_path);

    // Embedded texture — Assimp resolves the "*N" index via GetEmbeddedTexture
    const aiTexture *emb = scene->GetEmbeddedTexture(texture_path.C_Str());
    if (emb) {
      std::string name = std::string("__embedded__") + texture_path.C_Str();

      if (emb->mHeight == 0) {
        // Compressed bytes (PNG/JPG/etc.) — decode with stb
        int w, h, ch;
        stbi_uc *data = stbi_load_from_memory(
            reinterpret_cast<const stbi_uc *>(emb->pcData), emb->mWidth, &w, &h,
            &ch, 4);
        if (!data) {
          spdlog::warn("[TEXLOAD] stbi failed on embedded texture '{}': {}",
                       texture_path.C_Str(), stbi_failure_reason());
          return {};
        }
        return import_texture(name, data, (uint32_t)w, (uint32_t)h, 4);
      } else {
        // Raw ARGB8888 texels
        uint32_t pixel_count = emb->mWidth * emb->mHeight;
        uint8_t *data = static_cast<uint8_t *>(malloc(pixel_count * 4));
        for (uint32_t i = 0; i < pixel_count; ++i) {
          data[i * 4 + 0] = emb->pcData[i].r;
          data[i * 4 + 1] = emb->pcData[i].g;
          data[i * 4 + 2] = emb->pcData[i].b;
          data[i * 4 + 3] = emb->pcData[i].a;
        }
        return import_texture(name, data, emb->mWidth, emb->mHeight, 4);
      }
    }
    std::filesystem::path full_path =
        std::filesystem::path(model_directory) / texture_path.C_Str();
    spdlog::debug("[TEXLOAD] path='{}' exists={}", full_path.string(),
                  std::filesystem::exists(full_path));
    if (!std::filesystem::exists(full_path)) {
      spdlog::warn("Texture file not found: {}", full_path.string());
      return {};
    }
    return import_texture(full_path.string());
  }

  Material import_material(const aiScene *scene, const aiMaterial *ai_material,
                           const std::string &model_directory) {
    Material material{};

    aiString ai_name;
    if (ai_material->Get(AI_MATKEY_NAME, ai_name) == AI_SUCCESS) {
      material.name = ai_name.C_Str();
    }

    // GLTF PBR uses BASE_COLOR; legacy OBJ uses DIFFUSE — try both
    material.albedo = load_texture_from_material(
        scene, ai_material, aiTextureType_BASE_COLOR, model_directory);
    if (!material.albedo) {
      material.albedo = load_texture_from_material(
          scene, ai_material, aiTextureType_DIFFUSE, model_directory);
    }
    if (!material.albedo) {
      material.albedo = get_default_texture();
    }
    spdlog::debug("[MAT] '{}' albedo texture_id={} valid={}", material.name,
                  material.albedo.texture_id,
                  static_cast<bool>(material.albedo));

    material.normal = load_texture_from_material(
        scene, ai_material, aiTextureType_NORMALS, model_directory);

    material.metallic = load_texture_from_material(
        scene, ai_material, aiTextureType_METALNESS, model_directory);

    material.roughness = load_texture_from_material(
        scene, ai_material, aiTextureType_DIFFUSE_ROUGHNESS, model_directory);

    material.ao = load_texture_from_material(
        scene, ai_material, aiTextureType_AMBIENT_OCCLUSION, model_directory);

    return material;
  }

  void load_gltf_image(const fastgltf::Asset &asset,
                       const fastgltf::Image &image) {
    auto import_encoded_image = [&](const char *name, const void *data,
                                    std::size_t size) {
      int width = 0;
      int height = 0;
      int channels = 0;

      stbi_uc *pixels = stbi_load_from_memory(
          static_cast<const stbi_uc *>(data), static_cast<int>(size), &width,
          &height, &channels, STBI_rgb_alpha);

      if (!pixels) {
        throw std::runtime_error("Failed to decode glTF image.");
      }

      import_texture(name, pixels, width, height, 4);

      stbi_image_free(pixels);
    };

    std::visit(
        fastgltf::visitor{
            [](auto &) {
              throw std::runtime_error("Unsupported glTF image source.");
            },

            [&](const fastgltf::sources::URI &uri) {
              assert(uri.uri.isLocalPath());
              assert(uri.fileByteOffset == 0);

              const std::string path(uri.uri.path().begin(),
                                     uri.uri.path().end());
              import_texture(path);
            },

            [&](const fastgltf::sources::Array &array) {
              import_encoded_image(image.name.c_str(), array.bytes.data(),
                                   array.bytes.size());
            },

            [&](const fastgltf::sources::BufferView &source) {
              const auto &bufferView =
                  asset.bufferViews[source.bufferViewIndex];
              const auto &buffer = asset.buffers[bufferView.bufferIndex];

              std::visit(fastgltf::visitor{
                             [](auto &) {
                               throw std::runtime_error(
                                   "Unsupported glTF buffer source for image.");
                             },

                             [&](const fastgltf::sources::Array &array) {
                               const auto offset = bufferView.byteOffset;
                               const auto length = bufferView.byteLength;

                               if (offset + length > array.bytes.size()) {
                                 throw std::runtime_error(
                                     "Invalid glTF image bufferView range.");
                               }

                               import_encoded_image(image.name.c_str(),
                                                    array.bytes.data() + offset,
                                                    length);
                             },
                         },
                         buffer.data);
            },
        },
        image.data);
  }

  Material import_material(const fastgltf::Asset &asset,
                           const fastgltf::Material &gltf_material,
                           const std::string &model_directory) {
    Material material{};

    material.name = gltf_material.name;

    auto load_image_helper = [&](const auto &tex_info) {
      if (!tex_info.has_value()) {
        return;
      }

      auto texture = asset.textures[tex_info.value().textureIndex];
      assert(texture.imageIndex.has_value());
      const auto &image = asset.images[texture.imageIndex.value()];
      load_gltf_image(asset, image);

      using texture_type = std::decay_t<decltype(tex_info)>;

      if constexpr (std::is_same_v<texture_type, fastgltf::NormalTextureInfo>) {
        material.normal_scale = tex_info.scale;
      } else if constexpr (std::is_same_v<texture_type,
                                          fastgltf::OcclusionTextureInfo>) {
        material.occlusion_strength = tex_info.strength;
      }
    };

    // TODO
    // parse factors like basecolorfactor, metallic factor, rougness,
    // parse alpha mode as well
    const auto &bcf = gltf_material.pbrData.baseColorFactor;
    material.albedo_factor =
        glm::make_vec4(gltf_material.pbrData.baseColorFactor.data());
    material.metallic_factor = gltf_material.pbrData.metallicFactor;

    if (gltf_material.normalTexture.has_value()) {
      material.normal_scale = gltf_material.normalTexture.value().scale;
    }

    load_image_helper(gltf_material.pbrData.baseColorTexture);
    load_image_helper(gltf_material.pbrData.metallicRoughnessTexture);
    load_image_helper(gltf_material.normalTexture);
    // load_image_helper(gltf_material.pbrData.emissiveTexture);
    // load_image_helper(gltf_material.occlusionTexture);
    // load_image_helper(gltf_material.pbrData.emissiveTexture);

    if (gltf_material.pbrData.metallicRoughnessTexture.has_value()) {
      auto gltf_albedo_texture =
          asset.textures[gltf_material.pbrData.baseColorTexture.value()
                             .textureIndex];
      auto gltf_albedo_image = asset.images[gltf_albedo_texture.imageIndex];
    }

    // GLTF PBR uses BASE_COLOR; legacy OBJ uses DIFFUSE — try both
    material.albedo = load_texture_from_material(
        scene, ai_material, aiTextureType_BASE_COLOR, model_directory);
    if (!material.albedo) {
      material.albedo = load_texture_from_material(
          scene, ai_material, aiTextureType_DIFFUSE, model_directory);
    }
    if (!material.albedo) {
      material.albedo = get_default_texture();
    }
    spdlog::debug("[MAT] '{}' albedo texture_id={} valid={}", material.name,
                  material.albedo.texture_id,
                  static_cast<bool>(material.albedo));

    material.normal = load_texture_from_material(
        scene, ai_material, aiTextureType_NORMALS, model_directory);

    material.metallic = load_texture_from_material(
        scene, ai_material, aiTextureType_METALNESS, model_directory);

    material.roughness = load_texture_from_material(
        scene, ai_material, aiTextureType_DIFFUSE_ROUGHNESS, model_directory);

    material.ao = load_texture_from_material(
        scene, ai_material, aiTextureType_AMBIENT_OCCLUSION, model_directory);

    return material;
  }

private:
  TextureManager();
  ~TextureManager() = default;

  void create_default_texture();

  std::vector<TextureHandle> m_textures_to_upload_to_gpu{};
  std::unordered_map<TextureHandle, Texture> m_texture_map{};
  TextureHandle m_default_texture_handle{};
};
} // namespace Expectre

#endif // TEXTURE_MANAGER_H