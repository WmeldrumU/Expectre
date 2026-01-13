#ifndef MATH_UTILS_H
#define MATH_UTILS_H
#include <assimp/matrix4x4.h>
#include <assimp/mesh.h>

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp> // Include this header for glm::pi<float>()
#include <glm/gtc/matrix_transform.hpp> // Add this include to ensure glm::perspective is available
#include <glm/mat4x3.hpp>

#include <spdlog/spdlog.h>

namespace Expectre {
namespace MathUtils {

static glm::mat4 to_glm(const aiMatrix4x4 &m) {
  return glm::mat4(m.a1, m.b1, m.c1, m.d1, m.a2, m.b2, m.c2, m.d2, m.a3, m.b3,
                   m.c3, m.d3, m.a4, m.b4, m.c4, m.d4);
}

static glm::mat4x3 to_glm_4x3(const aiMatrix4x4 &m) {
  return glm::mat4x3(m.a1, m.b1, m.c1, m.d1, m.a2, m.b2, m.c2, m.d2, m.a3, m.b3,
                     m.c3, m.d3);
}

static void printMatrix4x4(const glm::mat4 &matrix) {
  spdlog::info("Matrix:\n");
  for (int i = 0; i < 4; ++i) {
    for (int j = 0; j < 4; ++j) {
      spdlog::info("{}", matrix[j][i]);
    }
    spdlog::info("");
  }
  spdlog::info("");
}

static void printMatrix4x3(const glm::mat4x3 &matrix) {
  spdlog::info("Matrix:\n");
  for (int i = 0; i < 4; ++i) {
    for (int j = 0; j < 3; ++j) {
      spdlog::info("{}", matrix[j][i]);
    }
    spdlog::info("");
  }
  spdlog::info("");
}
} // namespace MathUtils
} // namespace Expectre

#endif // MATH_UTILS_H