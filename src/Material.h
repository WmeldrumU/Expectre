#ifndef MATERIAL_H
#define MATERIAL_H
#include "Image.h"
#include <flecs.h>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <string>
namespace Expectre {

// ECS
struct UsesMaterial {};
struct Material {
  std::string name;

  // entities are Images

  // Color Data, Uses sRGB
  flecs::entity normal;
  flecs::entity emissive;

  // Non-Color Data (Linear / Doesn't use sRGB)
  flecs::entity albedo;
  flecs::entity metallic_roughness; // occlusion = red channel, metallic =
  flecs::entity occlusion;

  glm::vec4 albedo_factor = glm::vec4(1.0f);
  float metallic_factor = 1.0f;
  float roughness_factor = 1.0f;
  float normal_scale = 1.0f;
  float occlusion_strength = 1.0f;
  glm::vec3 emissive_factor = glm::vec3(0.0f);
  float emissive_strength = 1.0f;
};

} // namespace Expectre

#endif // MATERIAL_H