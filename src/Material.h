#ifndef MATERIAL_H
#define MATERIAL_H
#include "Texture.h"
#include <string>

namespace Expectre {

struct MaterialHandle {
  uint64_t material_id = -1;
  explicit operator bool() const { return material_id != -1; }

  // Equality operator for use in unordered_map
  bool operator==(const MaterialHandle &other) const {
    return material_id == other.material_id;
  }
};

struct Material {
  std::string name;
  TextureHandle albedo;
  TextureHandle normal;
  TextureHandle metallic;
  TextureHandle roughness;
  TextureHandle ao; // Ambient occlusion (optional)
};

} // namespace Expectre

// Specialize std::hash for MaterialHandle
namespace std {
template <> struct hash<Expectre::MaterialHandle> {
  std::size_t operator()(const Expectre::MaterialHandle &handle) const {
    return std::hash<uint64_t>{}(handle.material_id);
  }
};
} // namespace std

#endif // MATERIAL_H