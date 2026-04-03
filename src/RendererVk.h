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
#include "RenderResourceManager.h"
#include "RenderableInfo.h"
#include "ShaderFileWatcher.h"
#include "TextureVk.h"
#include "ToolsVk.h"
#include "input/InputManager.h"
#include "observer.h"

#include <memory>

#define MAX_CONCURRENT_FRAMES 2

// Default fence timeout in nanoseconds
#define DEFAULT_FENCE_TIMEOUT 100000000000

#define PRESENT_MODE VK_PRESENT_MODE_MAILBOX_KHR
namespace Expectre {

class Camera;
class NoesisUI; // forward-declared from noesis/NoesisUI.h

// Based on GTX 780 capabilites
static constexpr uint32_t kMaxDescriptorSetSamplers = 1048576;
static constexpr uint32_t kMaxDescriptorSetUniformBuffers = 90;
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
  RendererVk(VkInstance &instance, VkPhysicalDevice &physical_device,
             VkDevice &device, VmaAllocator &allocator, VkSurfaceKHR &surface,
             VkQueue &graphics_queue, uint32_t &graphics_queue_index,
             VkQueue &present_queue, uint32_t &present_queue_index,
             uint32_t width, uint32_t height, InputManager &input_manager);
  ~RendererVk();

  bool is_ready() { return m_ready; }
  void update(uint64_t delta_t);
  void draw_frame(const Camera &camera,
                  const std::vector<RenderableInfo> &renderables) override;
  void upload_texture_to_gpu(const Texture &texture);

  void
  upload_pending_assets(const std::vector<RenderableInfo> &pending_renderables);

  NoesisUI *GetNoesisUI() { return m_noesisUI.get(); }
  void OnWindowResize(glm::uvec2 new_dims);

private:
  void create_swapchain();

  VkCommandBuffer create_command_buffer(VkDevice device,
                                        VkCommandPool command_pool);

  VkImageView create_swapchain_and_image_views(VkDevice device, VkImage image,
                                               VkFormat format,
                                               VkImageAspectFlags flags);

  VkCommandPool create_command_pool(VkDevice device,
                                    uint32_t graphics_queue_family_index);

  /// Configuration for a single render pass.
  struct RenderPassConfig {
    VkFormat colorFormat;
    VkFormat depthFormat;
    VkAttachmentLoadOp colorLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    VkImageLayout colorInitialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    VkImageLayout colorFinalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    VkAttachmentLoadOp depthLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    VkAttachmentLoadOp stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    VkImageLayout depthInitialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    VkImageLayout depthFinalLayout =
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
  };

  VkRenderPass create_renderpass(VkDevice device,
                                 const RenderPassConfig &config);

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

  VkFramebuffer create_framebuffer(VkDevice device, VkRenderPass renderpass,
                                   VkImageView view,
                                   VkImageView depth_view = VK_NULL_HANDLE);

  void create_sync_objects();

  void record_draw_commands(VkCommandBuffer command_buffer,
                            uint32_t image_index,
                            const std::vector<RenderableInfo> &renderables);

  VkPipelineLayout
  create_pipeline_layout(VkDevice device,
                         VkDescriptorSetLayout descriptor_set_layout);

  VkDescriptorSetLayout create_descriptor_set_layout(
      const std::vector<VkDescriptorSetLayoutBinding> &layout_bindings);

  UniformBuffer create_uniform_buffer(VmaAllocator allocator,
                                      VkDeviceSize buffer_size);

  void update_uniform_buffer(const Camera &camera);

  void cleanup_swapchain_and_depth_stencil();

  void recreate_swapchain_and_depth_stencil();

  VkInstance &m_instance;
  VkPhysicalDevice &m_physical_device;
  VkDevice &m_device;

  VkQueue &m_graphics_queue;
  VkQueue &m_present_queue;

  VkSwapchainKHR m_swapchain{};
  std::vector<VkImage> m_swapchain_images{};
  VkFormat m_swapchain_image_format{};
  VkExtent2D m_extent{};
  VkExtent2D m_pending_extent{};
  std::vector<VkFramebuffer> m_swapchain_framebuffers{};
  std::vector<VkImageView> m_swapchain_image_views{};

  VkRenderPass m_render_pass{};
  VkRenderPass m_ui_render_pass{}; // Separate render pass for UI overlay
  std::vector<VkFramebuffer>
      m_ui_swapchain_framebuffers{}; // UI-specific framebuffers
  VkPipelineLayout m_pipeline_layout{};
  VkPipeline m_pipeline{};
  VkDescriptorPool m_descriptor_pool{};
  VkPipelineCache m_pipeline_cache = VK_NULL_HANDLE;
  VkDescriptorSetLayout m_descriptor_set_layout{VK_NULL_HANDLE};

  std::vector<VkSemaphore> m_available_image_semaphores{};
  std::vector<VkSemaphore> m_finished_render_semaphores{};
  std::vector<VkFence> m_in_flight_fences{};
  std::unique_ptr<RenderResourceManager> m_resource_manager;
  InputManager &m_input_manager;

  std::array<struct UniformBuffer, MAX_CONCURRENT_FRAMES> m_uniform_buffers{};
  VkPhysicalDeviceMemoryProperties m_phys_memory_properties{};
  VkCommandPool m_cmd_pool = VK_NULL_HANDLE;
  std::array<VkCommandBuffer, MAX_CONCURRENT_FRAMES> m_cmd_buffers;
  bool m_ready = false;

  TextureVk m_depth_stencil;

  TextureVk m_texture;

  VkSampler m_texture_sampler{};
  VkSurfaceFormatKHR m_surface_format{};

  uint32_t m_current_frame{0};
  VkDebugUtilsMessengerEXT m_debug_messenger{};

  float m_priority = 1.0f;
  bool m_layers_supported = false;

  std::unique_ptr<ShaderFileWatcher> m_vert_shader_watcher = nullptr;
  std::unique_ptr<ShaderFileWatcher> m_frag_shader_watcher = nullptr;

  std::unique_ptr<NoesisUI> m_noesisUI;
  std::shared_ptr<InputObserver> m_ns_input_adapter;
  uint64_t m_frameCounter = 0;
  double m_totalTimeSeconds = 0.0;

  VmaAllocator &m_allocator;
  VkSurfaceKHR &m_surface;
  uint32_t &m_graphics_queue_index;
  uint32_t &m_present_queue_index;

  bool m_window_resize_is_pending = false;
};

} // namespace Expectre
#endif // RENDERER_VK_H