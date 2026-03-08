#ifndef RENDERER_VK_H
#define RENDERER_VK_H

#include <vulkan/vulkan.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <array>
#include <assimp/Importer.hpp>
#include <glm/glm.hpp>
#include <optional>
#include <stdexcept>
#include <stdio.h>
#include <string>
#include <vector>

#include <vma/vk_mem_alloc.h>

#include "IRenderer.h"
#include "IUIRenderer.h"
#include "ShaderFileWatcher.h"
#include "TextureVk.h"
#include "ToolsVk.h"
#include "observer.h"
#include "scene/SceneObject.h"

#define MAX_CONCURRENT_FRAMES 2

// Default fence timeout in nanoseconds
#define DEFAULT_FENCE_TIMEOUT 100000000000

#define PRESENT_MODE VK_PRESENT_MODE_MAILBOX_KHR
namespace Expectre {

class Camera;

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

struct MVP_uniform_object {
  glm::mat4 model;
  glm::mat4 view;
  glm::mat4 projection;
};
struct UniformBuffer {
  AllocatedBuffer allocated_buffer{};
  // A descriptor set in Vulkan is a GPU-side object that binds your shader to
  // actual resources
  VkDescriptorSet descriptorSet{};
  // We keep a pointer to the mapped buffer, so we can easily update it's
  // contents via a memcpy
  uint8_t *mapped{nullptr};
};

class RendererVk : public IRenderer {

public:
  RendererVk() = delete;
  // --- Prevent copying ---
  // Delete the copy constructor
  RendererVk(const RendererVk &) = delete;
  // Delete the copy assignment operator
  RendererVk &operator=(const RendererVk &) = delete;
  RendererVk(VkPhysicalDevice &physical_device, VkDevice &device,
             VmaAllocator &allocator, VkSurfaceKHR &surface,
             VkQueue &graphics_queue, uint32_t &graphics_queue_index,
             VkQueue &present_queue, uint32_t &present_queue_index);
  ~RendererVk();

  bool is_ready() { return m_ready; }
  void update(uint64_t delta_t) override;
  void draw_frame(const Camera &camera) override;

  void upload_mesh_to_gpu(const Mesh &mesh);
  void upload_texture_to_gpu(const Texture &texture);

private:
  void create_swapchain();

  VkCommandBuffer create_command_buffer(VkDevice device,
                                        VkCommandPool command_pool);

  void create_geometry_buffer();

  VkImageView create_swapchain_image_views(VkDevice device, VkImage image,
                                           VkFormat format,
                                           VkImageAspectFlags flags);

  VkCommandPool create_command_pool(VkDevice device,
                                    uint32_t graphics_queue_family_index);

  VkRenderPass create_renderpass(VkDevice device, VkFormat color_format,
                                 VkFormat depth_format,
                                 bool is_presenting_pass);

  VkPipeline create_pipeline(VkDevice device, VkRenderPass renderpass,
                             VkPipelineLayout pipeline_layout);

  VkDescriptorPool
  create_descriptor_pool(VkDevice device,
                         std::vector<VkDescriptorPoolSize> pool_sizes,
                         uint32_t num_sets);

  VkDescriptorSet create_descriptor_set(VkDevice device,
                                        VkDescriptorPool descriptor_pool,
                                        VkDescriptorSetLayout descriptor_layout,
                                        VkBuffer buffer, VkImageView image_view,
                                        VkSampler sampler);

  VkFramebuffer create_framebuffer(VkDevice device, VkImageView view,
                                   VkImageView depth_view = VK_NULL_HANDLE);

  void create_sync_objects();

  void record_draw_commands(VkCommandBuffer command_buffer,
                            uint32_t image_index);

  VkPipelineLayout
  create_pipeline_layout(VkDevice device,
                         VkDescriptorSetLayout descriptor_set_layout);

  VkDescriptorSetLayout create_descriptor_set_layout(
      const std::vector<VkDescriptorSetLayoutBinding> &layout_bindings);

  UniformBuffer create_uniform_buffer(VmaAllocator allocator,
                                      VkDeviceSize buffer_size);

  void update_uniform_buffer(const Camera &camera);

  void cleanup_swapchain();

  void update_geometry_buffer(SceneObject &object);

  VkPhysicalDevice &m_physical_device;
  VkDevice &m_device;

  VkQueue &m_graphics_queue;
  VkQueue &m_present_queue;

  VkSwapchainKHR m_swapchain{};
  std::vector<VkImage> m_swapchain_images{};
  VkFormat m_swapchain_image_format{};
  VkExtent2D m_extent{};
  std::vector<VkFramebuffer> m_swapchain_framebuffers{};
  std::vector<VkImageView> m_swapchain_image_views{};

  VkRenderPass m_render_pass{};
  VkPipelineLayout m_pipeline_layout{};
  VkPipelineLayout m_ui_pipeline_layout{};
  VkPipeline m_pipeline{};
  VkPipeline m_ui_pipeline{};
  VkDescriptorPool m_descriptor_pool{};
  VkPipelineCache m_pipeline_cache = VK_NULL_HANDLE;
  VkDescriptorSetLayout m_descriptor_set_layout{VK_NULL_HANDLE};

  std::vector<VkSemaphore> m_available_image_semaphores{};
  std::vector<VkSemaphore> m_finished_render_semaphores{};
  std::vector<VkFence> m_in_flight_fences{};

  GeometryBuffer m_geometry_buffer{};
  std::vector<MeshAllocation> m_mesh_allocations{};
  std::array<struct UniformBuffer, MAX_CONCURRENT_FRAMES> m_uniform_buffers{};
  VkPhysicalDeviceMemoryProperties m_phys_memory_properties{};
  VkCommandPool m_cmd_pool = VK_NULL_HANDLE;
  // VkCommandBuffer m_cmd_buffer = VK_NULL_HANDLE;
  std::array<VkCommandBuffer, MAX_CONCURRENT_FRAMES> m_cmd_buffers;
  bool m_ready = false;

  TextureVk m_depth_stencil;

  TextureVk m_texture{};
  VkSampler m_texture_sampler{};
  VkSurfaceFormatKHR m_surface_format{};

  uint32_t m_current_frame{0};
  VkDebugUtilsMessengerEXT m_debug_messenger{};

  float m_priority = 1.0f;
  bool m_layers_supported = false;

  struct {
    float camera_speed = 1.0f;
    bool moveForward = false;
    bool moveBack = false;
    bool moveLeft = false;
    bool moveRight = false;
    glm::f32vec3 movement_dir = {0.0f, 0.0f, 0.0f};
    glm::f32vec3 pos = {0.0f, 1.0f, 2.0f};
    glm::f32vec3 forward_dir = {0.0f, 0.0f, -1.0f};

  } m_camera{};

  std::unique_ptr<ShaderFileWatcher> m_vert_shader_watcher = nullptr;
  std::unique_ptr<ShaderFileWatcher> m_frag_shader_watcher = nullptr;

  std::unique_ptr<IUIRenderer> m_ui_renderer = nullptr;

  VmaAllocator &m_allocator;
  VkSurfaceKHR &m_surface;
  uint32 &m_graphics_queue_index;
  uint32 &m_present_queue_index;
};

} // namespace Expectre
#endif // RENDERER_VK_H