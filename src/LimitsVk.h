#ifndef LIMITS_VK_H
#define LIMITS_VK_H
#include <cstdint>

namespace Expectre {
// Based on GTX 780 capabilites
static constexpr uint32_t kMaxDescriptorSetSamplers = 1048576;
static constexpr uint32_t kMaxDescriptorSetUniformBuffers = 90;
}
#endif