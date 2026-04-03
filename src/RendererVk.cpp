// Library macros
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#include "RendererVk.h"

#include <array>
#include <bitset>
#include <cassert>
#include <iostream>
#include <set>

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <spdlog/spdlog.h>

#include "AppTime.h"
#include "MaterialManager.h"
#include "Mesh.h"
#include "MeshManager.h"
#include "RenderableInfo.h"
#include "ShaderFileWatcher.h"
#include "TextureManager.h"
#include "ToolsVk.h"
#include "scene/MeshComponent.h"
#include "scene/TransformComponent.h"

#include "scene/Camera.h"

#include "noesis/NoesisUI.h"

namespace Expectre {

RendererVk::RendererVk(VkInstance &instance, VkPhysicalDevice &physical_device,
                       VkDevice &device, VmaAllocator &allocator,
                       VkSurfaceKHR &surface, VkQueue &graphics_queue,
                       uint32_t &graphics_queue_index, VkQueue &present_queue,
                       uint32_t &present_queue_index, uint32_t width,
                       uint32_t height, InputManager &input_manager)
    : m_instance{instance}, m_physical_device{physical_device},
      m_device{device}, m_allocator{allocator}, m_surface{surface},
      m_graphics_queue{graphics_queue},
      m_graphics_queue_index{graphics_queue_index},
      m_present_queue{present_queue},
      m_present_queue_index{present_queue_index}, m_extent{width, height},
      m_pending_extent{width, height}, m_input_manager{input_manager} {

  // Command buffers and swapchain
  create_swapchain();

  m_swapchain_image_views.resize(m_swapchain_images.size());
  for (auto i = 0; i < m_swapchain_images.size(); i++) {
    m_swapchain_image_views[i] = create_swapchain_and_image_views(
        device, m_swapchain_images[i], m_swapchain_image_format,
        VK_IMAGE_ASPECT_COLOR_BIT);
  }
  m_cmd_pool = create_command_pool(device, graphics_queue_index);
  m_resource_manager = std::make_unique<RenderResourceManager>(
      device, physical_device, allocator, graphics_queue_index, graphics_queue);

  m_depth_stencil =
      TextureVk::create_depth_stencil(m_physical_device, device, m_cmd_pool,
                                      m_graphics_queue, allocator, m_extent);
  RenderPassConfig rp_config{};
  rp_config.colorFormat = m_swapchain_image_format,
  rp_config.depthFormat = m_depth_stencil.image_info.format,
  rp_config.colorLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
  rp_config.colorInitialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
  rp_config.colorFinalLayout =
      VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, // For UI overlay
      rp_config.depthLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
  rp_config.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
  rp_config.depthInitialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
  // 3D render pass: outputs to COLOR_ATTACHMENT_OPTIMAL for UI to overlay on
      m_render_pass = create_renderpass(device, rp_config);

  // Create separate UI render pass (loads 3D output, presents)
  // NOTE: Must include depth attachment for framebuffer compatibility
  // (even though UI doesn't use it)
  RenderPassConfig ui_rp_config{};
  ui_rp_config.colorFormat = m_swapchain_image_format,
  ui_rp_config.depthFormat = m_depth_stencil.image_info.format,
  ui_rp_config.colorLoadOp = VK_ATTACHMENT_LOAD_OP_LOAD, // Load from 3D pass
      ui_rp_config.colorInitialLayout =
          VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
  ui_rp_config.colorFinalLayout =
      VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, // Ready to present
      ui_rp_config.depthLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
  ui_rp_config.stencilLoadOp =
      VK_ATTACHMENT_LOAD_OP_CLEAR, // Noesis uses stencil for clipping
      ui_rp_config.depthInitialLayout =
          VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
  ui_rp_config.depthFinalLayout =
      VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
  m_ui_render_pass = create_renderpass(device, ui_rp_config);

  // === DESCRIPTOR SET LAYOUT BINDINGS ===
  // Binding 0: MVP uniform buffer
  VkDescriptorSetLayoutBinding ubo_layout_binding{};
  ubo_layout_binding.binding = 0;
  ubo_layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  ubo_layout_binding.descriptorCount = 1;
  ubo_layout_binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
  ubo_layout_binding.pImmutableSamplers = nullptr;

  // Binding 1: Bindless texture array
  // Large descriptor count enables dynamic texture indexing in shaders
  // Paired with UPDATE_AFTER_BIND + PARTIALLY_BOUND flags for bindless support
  VkDescriptorSetLayoutBinding sampler_layout_binding{};
  sampler_layout_binding.binding = 1;
  sampler_layout_binding.descriptorCount = kMaxDescriptorSetSamplers;
  sampler_layout_binding.descriptorType =
      VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  sampler_layout_binding.pImmutableSamplers = nullptr;
  sampler_layout_binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

  m_descriptor_set_layout = create_descriptor_set_layout(
      {ubo_layout_binding, sampler_layout_binding});
  m_pipeline_layout = create_pipeline_layout(device, m_descriptor_set_layout);
  m_pipeline = create_pipeline(device, m_render_pass, m_pipeline_layout);
  m_swapchain_framebuffers.resize(m_swapchain_image_views.size());
  for (auto i = 0; i < m_swapchain_image_views.size(); i++) {
    m_swapchain_framebuffers[i] =
        create_framebuffer(device, m_render_pass, m_swapchain_image_views[i],
                           m_depth_stencil.view);
  }

  // Create UI-specific framebuffers (also compatible with both render passes)
  m_ui_swapchain_framebuffers.resize(m_swapchain_image_views.size());
  for (auto i = 0; i < m_swapchain_image_views.size(); i++) {
    m_ui_swapchain_framebuffers[i] =
        create_framebuffer(device, m_ui_render_pass, m_swapchain_image_views[i],
                           m_depth_stencil.view);
  }
  m_texture = TextureVk::create_texture_from_file(
      m_device, m_cmd_pool, m_graphics_queue, m_allocator,
      WORKSPACE_DIR + std::string("/assets/teapot/brick.png"));

  // Create a valid fallback texture so descriptor binding 1 is never null
  static const uint8_t white_pixel[] = {0, 255, 0, 255};

  // m_texture = TextureVk::create_texture(
  //     device, m_cmd_pool, m_graphics_queue, allocator, white_pixel, 1, 1, 1,
  //     VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT, 0,
  //     VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
  //     "__fallback_white_or_black_1x1");
  m_texture_sampler =
      ToolsVk::create_texture_sampler(m_physical_device, device);
  m_resource_manager->create_vertex_buffer(1024 * 1024 *
                                           48); // 48 MB for vertices
  m_resource_manager->create_index_buffer(1024 * 1024 * 16); // 16 MB for indices

  std::vector<VkDescriptorPoolSize> pool_sizes(2);
  pool_sizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  pool_sizes[0].descriptorCount = static_cast<uint32_t>(MAX_CONCURRENT_FRAMES);
  pool_sizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  pool_sizes[1].descriptorCount = static_cast<uint32_t>(MAX_CONCURRENT_FRAMES);
  m_descriptor_pool =
      create_descriptor_pool(device, pool_sizes, MAX_CONCURRENT_FRAMES);

  for (auto i = 0; i < MAX_CONCURRENT_FRAMES; i++) {
    auto &uniform_buffer = m_uniform_buffers[i];
    uniform_buffer =
        create_uniform_buffer(allocator, sizeof(MVP_uniform_object));
    uniform_buffer.descriptorSet = create_descriptor_set(
        device, m_descriptor_pool, m_descriptor_set_layout,
        m_uniform_buffers[i].allocated_buffer.buffer, m_texture.view,
        m_texture_sampler);

    m_cmd_buffers[i] = create_command_buffer(device, m_cmd_pool);
  }

  // Synchronization
  create_sync_objects();

  m_frag_shader_watcher = std::make_unique<ShaderFileWatcher>(
      ShaderFileWatcher(std::string(WORKSPACE_DIR) + "/shaders/frag.frag"));
  m_vert_shader_watcher = std::make_unique<ShaderFileWatcher>(
      ShaderFileWatcher(std::string(WORKSPACE_DIR) + "/shaders/vert.vert"));

  NoesisUI::InitInfo nsInit{};
  nsInit.instance = m_instance;
  nsInit.physicalDevice = m_physical_device;
  nsInit.device = m_device;
  nsInit.graphicsQueue = m_graphics_queue;
  nsInit.queueFamilyIndex = m_graphics_queue_index;
  nsInit.renderPass = m_ui_render_pass; // Use UI render pass for Noesis
  nsInit.sampleCount = VK_SAMPLE_COUNT_1_BIT;
  nsInit.width = m_extent.width;
  nsInit.height = m_extent.height;
  nsInit.maxFramesInFlight = MAX_CONCURRENT_FRAMES;

  m_noesisUI = std::make_unique<NoesisUI>(nsInit);

  // Register Noesis input adapter with the engine's InputManager.
  // The adapter is an InputObserver (SDL-only interface) — no Noesis types
  // leak into the core engine.
  m_ns_input_adapter = m_noesisUI->CreateInputAdapter();
  input_manager.AddObserver(m_ns_input_adapter);

  // Update NoesisUI to use the UI render pass (separate from 3D scene)
  // This requires calling WarmUpRenderPass with the UI-specific render pass
  // (This is handled in the NoesisUI constructor via InitInfo)
  m_ready = true;
}

void RendererVk::cleanup_swapchain_and_depth_stencil() {
  // Destroy depth buffer
  vkDestroyImageView(m_device, m_depth_stencil.view, nullptr);
  vmaDestroyImage(m_allocator, m_depth_stencil.image,
                  m_depth_stencil.allocation);

  for (auto framebuffer : m_swapchain_framebuffers) {
    vkDestroyFramebuffer(m_device, framebuffer, nullptr);
  }

  for (auto framebuffer : m_ui_swapchain_framebuffers) {
    vkDestroyFramebuffer(m_device, framebuffer, nullptr);
  }

  for (auto imageView : m_swapchain_image_views) {
    vkDestroyImageView(m_device, imageView, nullptr);
  }

  vkDestroySwapchainKHR(m_device, m_swapchain, nullptr);
}

RendererVk::~RendererVk() {

  vkDeviceWaitIdle(m_device);

  // Noesis cleanup (before Vulkan resource destruction)
  m_noesisUI.reset();

  // Destroy synchronization objects
  for (size_t i = 0; i < MAX_CONCURRENT_FRAMES; ++i) {
    vkDestroySemaphore(m_device, m_available_image_semaphores[i], nullptr);
    vkDestroyFence(m_device, m_in_flight_fences[i], nullptr);
  }
  for (size_t i = 0; i < m_swapchain_images.size(); i++) {
    vkDestroySemaphore(m_device, m_finished_render_semaphores[i], nullptr);
  }

  vkDestroyDescriptorSetLayout(m_device, m_descriptor_set_layout, nullptr);
  // Destroy descriptor pool
  vkDestroyDescriptorPool(m_device, m_descriptor_pool, nullptr);

  // Destroy uniform buffers
  for (auto &ub : m_uniform_buffers) {
    vkDestroyBuffer(m_device, ub.allocated_buffer.buffer, nullptr);
    vmaUnmapMemory(m_allocator, ub.allocated_buffer.allocation);
    vmaFreeMemory(m_allocator, ub.allocated_buffer.allocation);
  }

  // Destroy vertex and index buffer

  // vmaDestroyBuffer(m_allocator, m_geometry_buffer.buffer,
  //                  m_geometry_buffer.allocation);

  // Destroy pipeline and related layouts
  vkDestroyPipeline(m_device, m_pipeline, nullptr);
  vkDestroyPipelineLayout(m_device, m_pipeline_layout, nullptr);
  vkDestroyRenderPass(m_device, m_render_pass, nullptr);
  vkDestroyRenderPass(m_device, m_ui_render_pass,
                      nullptr); // Clean up UI render pass

  // Destroy texture resources
  vkDestroySampler(m_device, m_texture_sampler, nullptr);
  vkDestroyImageView(m_device, m_texture.view, nullptr);
  vkDestroyImage(m_device, m_texture.image, nullptr);
  vmaFreeMemory(m_allocator, m_texture.allocation);

  // Destroy command pool
  vkDestroyCommandPool(m_device, m_cmd_pool, nullptr);

  // vkDestroyImageView(m_device, m_depth_stencil.view, nullptr);
  // vmaDestroyImage(m_allocator, m_depth_stencil.image,
  //                 m_depth_stencil.allocation);

  cleanup_swapchain_and_depth_stencil();

  VmaTotalStatistics stats{};
  vmaCalculateStatistics(m_allocator, &stats);

  // Or a human-readable string:
  char *statsStr = nullptr;
  vmaBuildStatsString(m_allocator, &statsStr, VK_TRUE);
  // log statsStr somewhere:
  printf("%s\n", statsStr);
  vmaFreeStatsString(m_allocator, statsStr);
}

void RendererVk::create_swapchain() {
  ToolsVk::SwapChainSupportDetails swapchain_support_details =
      ToolsVk::query_swap_chain_support(m_physical_device, m_surface);

  m_surface_format =
      ToolsVk::choose_swap_surface_format(swapchain_support_details.formats);

  // Clamp extent to surface capabilities (defensive check)
  // clamped to max(minimum_supported, min(desired, max_supported))
  VkExtent2D clamped_extent = m_extent;
  clamped_extent.width = std::max(
      swapchain_support_details.capabilities.minImageExtent.width,
      std::min(m_extent.width,
               swapchain_support_details.capabilities.maxImageExtent.width));
  clamped_extent.height = std::max(
      swapchain_support_details.capabilities.minImageExtent.height,
      std::min(m_extent.height,
               swapchain_support_details.capabilities.maxImageExtent.height));

  // uint32_t image_count =
  // swapchain_support_details.capabilities.minImageCount
  // + 1;
  uint32_t image_count = MAX_CONCURRENT_FRAMES;
  if (swapchain_support_details.capabilities.maxImageCount > 0 &&
      image_count > swapchain_support_details.capabilities.maxImageCount) {
    image_count = swapchain_support_details.capabilities.maxImageCount;
  }

  VkSwapchainCreateInfoKHR create_info{};
  create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
  create_info.surface = m_surface;
  create_info.minImageCount = image_count;
  create_info.imageFormat = m_surface_format.format;
  create_info.imageColorSpace = m_surface_format.colorSpace;
  create_info.imageExtent = clamped_extent;
  create_info.imageArrayLayers = 1;
  create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

  ToolsVk::QueueFamilyIndices indices =
      ToolsVk::findQueueFamilies(m_physical_device, m_surface);
  uint32_t queue_family_indices[] = {indices.graphicsFamily.value(),
                                     indices.presentFamily.value()};

  if (indices.graphicsFamily != indices.presentFamily) {
    create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
    create_info.queueFamilyIndexCount = 2;
    create_info.pQueueFamilyIndices = queue_family_indices;
  } else {
    create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
  }

  create_info.preTransform =
      swapchain_support_details.capabilities.currentTransform;
  create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  create_info.presentMode = PRESENT_MODE;
  create_info.clipped = VK_TRUE;

  VK_CHECK_RESULT(
      vkCreateSwapchainKHR(m_device, &create_info, nullptr, &m_swapchain));

  vkGetSwapchainImagesKHR(m_device, m_swapchain, &image_count, nullptr);
  m_swapchain_images.resize(image_count);
  vkGetSwapchainImagesKHR(m_device, m_swapchain, &image_count,
                          m_swapchain_images.data());

  m_swapchain_image_format = m_surface_format.format;
}

VkImageView RendererVk::create_swapchain_and_image_views(
    VkDevice device, VkImage image, VkFormat format, VkImageAspectFlags flags) {
  VkImageView image_view;

  image_view = ToolsVk::create_image_view(device, image,
                                          m_swapchain_image_format, flags);

  return image_view;
}

VkRenderPass RendererVk::create_renderpass(VkDevice device,
                                           const RenderPassConfig &config) {
  VkAttachmentDescription color{};
  color.format = config.colorFormat;
  color.samples = VK_SAMPLE_COUNT_1_BIT;
  color.loadOp = config.colorLoadOp;
  color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  color.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  color.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  color.initialLayout = config.colorInitialLayout;
  color.finalLayout = config.colorFinalLayout;

  VkAttachmentDescription depth{};
  depth.format = config.depthFormat;
  depth.samples = VK_SAMPLE_COUNT_1_BIT;
  depth.loadOp = config.depthLoadOp;
  depth.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  depth.stencilLoadOp = config.stencilLoadOp;
  depth.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  depth.initialLayout = config.depthInitialLayout;
  depth.finalLayout = config.depthFinalLayout;

  VkAttachmentReference colorRef{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
  VkAttachmentReference depthRef{
      1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};

  VkSubpassDescription sub{};
  sub.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  sub.colorAttachmentCount = 1;
  sub.pColorAttachments = &colorRef;
  sub.pDepthStencilAttachment =
      (config.depthFormat != VK_FORMAT_UNDEFINED) ? &depthRef : nullptr;

  std::vector<VkAttachmentDescription> atts = {color};
  if (config.depthFormat != VK_FORMAT_UNDEFINED)
    atts.push_back(depth);

  // Subpass dependencies for proper synchronization with external operations
  VkSubpassDependency deps[2] = {};

  // Dependency at the start: external → subpass 0
  deps[0].srcSubpass = VK_SUBPASS_EXTERNAL;
  deps[0].dstSubpass = 0;
  deps[0].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                         VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
  deps[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                         VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
  deps[0].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
  deps[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
                          VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                          VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                          VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
  deps[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

  // Dependency at the end: subpass 0 → external
  deps[1].srcSubpass = 0;
  deps[1].dstSubpass = VK_SUBPASS_EXTERNAL;
  deps[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                         VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
  deps[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
  deps[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                          VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
  deps[1].dstAccessMask = 0;
  deps[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

  VkRenderPass render_pass;
  VkRenderPassCreateInfo rpci{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
  rpci.attachmentCount = (uint32_t)atts.size();
  rpci.pAttachments = atts.data();
  rpci.subpassCount = 1;
  rpci.pSubpasses = &sub;
  rpci.dependencyCount = 2;
  rpci.pDependencies = deps;
  VK_CHECK_RESULT(vkCreateRenderPass(device, &rpci, nullptr, &render_pass));
  return render_pass;
}

VkPipeline RendererVk::create_pipeline(VkDevice device, VkRenderPass renderpass,
                                       VkPipelineLayout pipeline_layout) {
  VkShaderModule vert_shader_module = ToolsVk::createShaderModule(
      device, (WORKSPACE_DIR + std::string("/shaders/vert.spv")));
  VkShaderModule frag_shader_module = ToolsVk::createShaderModule(
      device, (WORKSPACE_DIR + std::string("/shaders/frag.spv")));

  VkPipelineShaderStageCreateInfo vert_shader_stage_info{};
  vert_shader_stage_info.sType =
      VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  vert_shader_stage_info.stage = VK_SHADER_STAGE_VERTEX_BIT;
  vert_shader_stage_info.module = vert_shader_module;
  vert_shader_stage_info.pName = "main";

  VkPipelineShaderStageCreateInfo frag_shader_stage_info{};
  frag_shader_stage_info.sType =
      VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  frag_shader_stage_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
  frag_shader_stage_info.module = frag_shader_module;
  frag_shader_stage_info.pName = "main";

  VkPipelineShaderStageCreateInfo shader_stages[] = {vert_shader_stage_info,
                                                     frag_shader_stage_info};

  VkVertexInputBindingDescription binding_description{};
  binding_description.binding = 0;
  binding_description.stride = sizeof(Vertex);
  binding_description.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

  std::array<VkVertexInputAttributeDescription, 4>
      vertex_attribute_description{};
  vertex_attribute_description[0].binding = 0;
  vertex_attribute_description[0].location = 0;
  vertex_attribute_description[0].format = VK_FORMAT_R32G32B32_SFLOAT;
  vertex_attribute_description[0].offset = offsetof(Vertex, pos);

  vertex_attribute_description[1].binding = 0;
  vertex_attribute_description[1].location = 1;
  vertex_attribute_description[1].format = VK_FORMAT_R32G32B32_SFLOAT;
  vertex_attribute_description[1].offset = offsetof(Vertex, color);

  vertex_attribute_description[2].binding = 0;
  vertex_attribute_description[2].location = 2;
  vertex_attribute_description[2].format = VK_FORMAT_R32G32B32_SFLOAT;
  vertex_attribute_description[2].offset = offsetof(Vertex, normal);

  vertex_attribute_description[3].binding = 0;
  vertex_attribute_description[3].location = 3;
  vertex_attribute_description[3].format = VK_FORMAT_R32G32_SFLOAT;
  vertex_attribute_description[3].offset = offsetof(Vertex, tex_coord);

  VkPipelineVertexInputStateCreateInfo vertex_input_info{};
  vertex_input_info.sType =
      VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
  vertex_input_info.vertexBindingDescriptionCount = 1;
  vertex_input_info.vertexAttributeDescriptionCount =
      static_cast<uint32_t>(vertex_attribute_description.size());
  vertex_input_info.pVertexBindingDescriptions = &binding_description;
  vertex_input_info.pVertexAttributeDescriptions =
      vertex_attribute_description.data();

  VkPipelineInputAssemblyStateCreateInfo input_assembly{};
  input_assembly.sType =
      VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  input_assembly.primitiveRestartEnable = VK_FALSE;

  VkPipelineViewportStateCreateInfo viewport_state{};
  viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  viewport_state.viewportCount = 1;
  viewport_state.scissorCount = 1;

  VkPipelineRasterizationStateCreateInfo rasterizer{};
  rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  rasterizer.depthClampEnable = VK_FALSE;
  rasterizer.rasterizerDiscardEnable = VK_FALSE;
  rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
  rasterizer.lineWidth = 1.0f;
  rasterizer.cullMode = VK_CULL_MODE_NONE;
  rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
  rasterizer.depthBiasEnable = VK_FALSE;

  VkPipelineMultisampleStateCreateInfo multi_sample_info{};
  multi_sample_info.sType =
      VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  multi_sample_info.sampleShadingEnable = VK_FALSE;
  multi_sample_info.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

  VkPipelineColorBlendAttachmentState color_blend_attach{};
  color_blend_attach.colorWriteMask =
      VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
      VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
  color_blend_attach.blendEnable = VK_FALSE;

  VkPipelineColorBlendStateCreateInfo color_blend_info{};
  color_blend_info.sType =
      VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  color_blend_info.logicOpEnable = VK_FALSE;
  color_blend_info.logicOp = VK_LOGIC_OP_COPY;
  color_blend_info.attachmentCount = 1;
  color_blend_info.pAttachments = &color_blend_attach;
  color_blend_info.blendConstants[0] = 0.0f;
  color_blend_info.blendConstants[1] = 0.0f;
  color_blend_info.blendConstants[2] = 0.0f;
  color_blend_info.blendConstants[3] = 0.0f;

  std::vector<VkDynamicState> dynamic_states = {VK_DYNAMIC_STATE_VIEWPORT,
                                                VK_DYNAMIC_STATE_SCISSOR};
  VkPipelineDynamicStateCreateInfo dynamic_state{};
  dynamic_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
  dynamic_state.dynamicStateCount =
      static_cast<uint32_t>(dynamic_states.size());
  dynamic_state.pDynamicStates = dynamic_states.data();

  VkPipelineDepthStencilStateCreateInfo depth_stencil_state_info{};
  depth_stencil_state_info.sType =
      VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
  depth_stencil_state_info.depthTestEnable = VK_TRUE;
  depth_stencil_state_info.depthWriteEnable = VK_TRUE;
  depth_stencil_state_info.depthCompareOp = VK_COMPARE_OP_LESS;
  depth_stencil_state_info.depthBoundsTestEnable = VK_FALSE;
  depth_stencil_state_info.stencilTestEnable = VK_FALSE;

  VkGraphicsPipelineCreateInfo pipeline_info{};
  pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  pipeline_info.stageCount = 2;
  pipeline_info.pStages = shader_stages;
  pipeline_info.pVertexInputState = &vertex_input_info;
  pipeline_info.pInputAssemblyState = &input_assembly;
  pipeline_info.pViewportState = &viewport_state;
  pipeline_info.pRasterizationState = &rasterizer;
  pipeline_info.pDepthStencilState = &depth_stencil_state_info;
  pipeline_info.pMultisampleState = &multi_sample_info;
  pipeline_info.pColorBlendState = &color_blend_info;
  pipeline_info.pDynamicState = &dynamic_state;
  pipeline_info.layout = pipeline_layout;
  pipeline_info.renderPass = renderpass;
  pipeline_info.subpass = 0;
  pipeline_info.basePipelineHandle = VK_NULL_HANDLE;

  VkPipeline pipeline;
  VK_CHECK_RESULT(vkCreateGraphicsPipelines(
      device, VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &pipeline));

  vkDestroyShaderModule(device, frag_shader_module, nullptr);
  vkDestroyShaderModule(device, vert_shader_module, nullptr);

  return pipeline;
}

VkCommandPool
RendererVk::create_command_pool(VkDevice device,
                                uint32_t graphics_queue_family_index) {

  VkCommandPoolCreateInfo pool_info{};
  pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
  pool_info.queueFamilyIndex = graphics_queue_family_index;

  VkCommandPool command_pool{};
  VK_CHECK_RESULT(
      vkCreateCommandPool(device, &pool_info, nullptr, &command_pool));
  return command_pool;
}

VkCommandBuffer RendererVk::create_command_buffer(VkDevice device,
                                                  VkCommandPool command_pool) {
  VkCommandBufferAllocateInfo cmd_buf_info{};
  cmd_buf_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  cmd_buf_info.commandPool = command_pool;
  cmd_buf_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  cmd_buf_info.commandBufferCount = 1;

  VkCommandBuffer command_buffer{};
  VK_CHECK_RESULT(
      vkAllocateCommandBuffers(device, &cmd_buf_info, &command_buffer));
  return command_buffer;
}

VkDescriptorPool
RendererVk::create_descriptor_pool(VkDevice device,
                                   std::vector<VkDescriptorPoolSize> pool_sizes,
                                   uint32_t max_sets) {

  VkDescriptorPoolCreateInfo pool_info{};
  pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  pool_info.pNext = nullptr;
  pool_info.poolSizeCount = static_cast<uint32_t>(pool_sizes.size());
  pool_info.pPoolSizes = pool_sizes.data();
  pool_info.maxSets = max_sets;

  VkDescriptorPool pool;
  VK_CHECK_RESULT(vkCreateDescriptorPool(device, &pool_info, nullptr, &pool));
  return pool;
}

VkDescriptorSet RendererVk::create_descriptor_set(
    VkDevice device, VkDescriptorPool descriptor_pool,
    VkDescriptorSetLayout descriptor_set_layout, VkBuffer buffer,
    VkImageView image_view, VkSampler sampler) {

  VkDescriptorSet descriptor_set;

  VkDescriptorSetAllocateInfo alloc_info{};
  alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  alloc_info.descriptorPool = descriptor_pool;
  alloc_info.descriptorSetCount = 1;
  alloc_info.pSetLayouts = &descriptor_set_layout;

  VK_CHECK_RESULT(
      vkAllocateDescriptorSets(device, &alloc_info, &descriptor_set));

  VkDescriptorBufferInfo buffer_info{};
  buffer_info.buffer = buffer;
  buffer_info.offset = 0;
  buffer_info.range = sizeof(MVP_uniform_object);

  VkDescriptorImageInfo image_info{};
  image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  image_info.imageView = image_view;
  image_info.sampler = sampler;

  // VkWriteDescriptorSet - Represents a descriptor set write operation
  std::array<VkWriteDescriptorSet, 2> descriptor_writes{};
  descriptor_writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  descriptor_writes[0].dstSet = descriptor_set;
  descriptor_writes[0].dstBinding = 0;
  descriptor_writes[0].dstArrayElement = 0;
  descriptor_writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  descriptor_writes[0].descriptorCount = 1;
  descriptor_writes[0].pBufferInfo = &buffer_info;

  descriptor_writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  descriptor_writes[1].dstSet = descriptor_set;
  descriptor_writes[1].dstBinding = 1;
  descriptor_writes[1].dstArrayElement = 0;
  descriptor_writes[1].descriptorType =
      VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  descriptor_writes[1].descriptorCount = 1;
  descriptor_writes[1].pImageInfo = &image_info;

  vkUpdateDescriptorSets(device, descriptor_writes.size(),
                         descriptor_writes.data(), 0, nullptr);

  return descriptor_set;
}

VkFramebuffer RendererVk::create_framebuffer(VkDevice device,
                                             VkRenderPass renderpass,
                                             VkImageView view,
                                             VkImageView depth_view) {
  VkResult err;
  VkFramebuffer framebuffer;
  std::vector<VkImageView> attachments{view};

  if (depth_view != VK_NULL_HANDLE) {
    attachments.push_back(depth_view);
  }

  VkFramebufferCreateInfo framebuffer_info{};
  framebuffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
  framebuffer_info.renderPass = renderpass; // Use provided render pass
  framebuffer_info.attachmentCount = static_cast<uint32_t>(attachments.size());
  framebuffer_info.pAttachments = attachments.data();
  framebuffer_info.width = m_extent.width;
  framebuffer_info.height = m_extent.height;
  framebuffer_info.layers = 1;

  // Create framebuffer
  VK_CHECK_RESULT(
      vkCreateFramebuffer(device, &framebuffer_info, nullptr, &framebuffer));
  return framebuffer;
}

void RendererVk::create_sync_objects() {
  // === SYNCHRONIZATION PRIMITIVE COUNTS ===
  // Different counts because they serve different purposes:
  //
  // Frames-in-flight (typically 2): CPU can prepare frame N+1 while GPU
  // renders frame N Swapchain images (typically 2-3): The actual framebuffers
  // we render into
  //
  // We need:
  // - Fences per frame-in-flight (CPU waits for GPU to finish frame N before
  // reusing resources)
  // - Acquire semaphores per frame-in-flight (signals when we get an image
  // from swapchain)
  // - Render finished semaphores per swapchain IMAGE (each image needs its
  // own to prevent reuse)

  m_finished_render_semaphores.resize(m_swapchain_images.size()); // Per image
  m_available_image_semaphores.resize(MAX_CONCURRENT_FRAMES);     // Per frame
  m_in_flight_fences.resize(MAX_CONCURRENT_FRAMES);               // Per frame

  VkSemaphoreCreateInfo semaphore_info{};
  semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

  VkFenceCreateInfo fence_info{};
  fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT; // Start signaled so first
                                                   // frame doesn't wait

  // Create per-frame synchronization objects
  for (auto i = 0; i < MAX_CONCURRENT_FRAMES; i++) {
    // Acquire semaphore: Signaled when swapchain gives us an image
    auto avail = vkCreateSemaphore(m_device, &semaphore_info, nullptr,
                                   &m_available_image_semaphores[i]);
    // Fence: CPU waits on this to know when frame is completely done
    auto fences =
        vkCreateFence(m_device, &fence_info, nullptr, &m_in_flight_fences[i]);
    VK_CHECK_RESULT(avail);
    VK_CHECK_RESULT(fences);
  }

  // Create per-swapchain-image synchronization objects
  // Each swapchain image needs its own render finished semaphore because:
  // - Images can be acquired in any order (e.g., 0,1,0,2,1)
  // - We must not signal the same semaphore twice while presentation is using
  // it
  for (size_t i = 0; i < m_swapchain_images.size(); i++) {
    auto finished = vkCreateSemaphore(m_device, &semaphore_info, nullptr,
                                      &m_finished_render_semaphores[i]);
    VK_CHECK_RESULT(finished);
  }
}

void RendererVk::record_draw_commands(
    VkCommandBuffer command_buffer, uint32_t image_index,
    const std::vector<RenderableInfo> &renderables) {
  VkCommandBufferBeginInfo begin_info{};
  begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  begin_info.pNext = nullptr;
  begin_info.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
  begin_info.pInheritanceInfo = nullptr;

  VK_CHECK_RESULT(vkBeginCommandBuffer(command_buffer, &begin_info));

  // === Noesis pre-pass (offscreen effects, texture uploads) ===
  // Must run BEFORE the render pass so Noesis can do its offscreen work.
  if (m_noesisUI) {
    m_noesisUI->PreRender(command_buffer, m_frameCounter, m_totalTimeSeconds);
  }

  // === Single render pass: 3D scene + UI overlay ===
  std::array<VkClearValue, 2> clear_col;
  clear_col[0] = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
  clear_col[1].depthStencil = {1.0f, 0};
  VkRenderPassBeginInfo renderpass_info = {};
  renderpass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  renderpass_info.pNext = nullptr;
  renderpass_info.renderPass = m_render_pass;
  renderpass_info.framebuffer = m_swapchain_framebuffers[image_index];
  renderpass_info.renderArea.offset.x = 0;
  renderpass_info.renderArea.offset.y = 0;
  renderpass_info.renderArea.extent.width = m_extent.width;
  renderpass_info.renderArea.extent.height = m_extent.height;
  renderpass_info.clearValueCount = 2;
  renderpass_info.pClearValues = clear_col.data();

  vkCmdBeginRenderPass(command_buffer, &renderpass_info,
                       VK_SUBPASS_CONTENTS_INLINE);

  vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                    m_pipeline);

  VkViewport viewport{};
  viewport.x = 0.0f;
  viewport.y = 0.0f;
  viewport.width = (float)m_extent.width;
  viewport.height = (float)m_extent.height;
  viewport.minDepth = 0.0f;
  viewport.maxDepth = 1.0f;

  vkCmdSetViewport(command_buffer, 0, 1, &viewport);

  VkRect2D scissor{};
  scissor.offset.x = 0;
  scissor.offset.y = 0;
  scissor.extent.width = m_extent.width;
  scissor.extent.height = m_extent.height;

  vkCmdSetScissor(command_buffer, 0, 1, &scissor);

  VkDeviceSize offsets[] = {0};
  const auto &index_buffer = m_resource_manager->get_index_buffer();
  const auto &vertex_buffer = m_resource_manager->get_vertex_buffer();

  vkCmdBindVertexBuffers(command_buffer, 0, 1, &vertex_buffer.buffer, offsets);

  vkCmdBindIndexBuffer(command_buffer, index_buffer.buffer, 0 /*offset*/,
                       VK_INDEX_TYPE_UINT32);

  vkCmdBindDescriptorSets(
      command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline_layout, 0, 1,
      &m_uniform_buffers[m_current_frame].descriptorSet, 0, nullptr);

  const std::vector<MeshAllocation> &mesh_allocations =
      m_resource_manager->get_mesh_allocations();
  for (const auto &alloc : mesh_allocations) {
    // Check if this mesh's material has an albedo texture
    uint32_t useTexture = 0u;
    auto mat = MaterialManager::Instance().get_material(alloc.material);
    if (mat.has_value() && mat->get().albedo) {
      useTexture = 1u;
    }
    vkCmdPushConstants(command_buffer, m_pipeline_layout,
                       VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(uint32_t),
                       &useTexture);
    vkCmdDrawIndexed(command_buffer, alloc.index_count, 1, alloc.index_offset,
                     alloc.vertex_offset, 0 /* first instance */);
  }

  vkCmdEndRenderPass(command_buffer);

  // === Separate UI Render Pass ===
  if (m_noesisUI) {
    // Dummy clear values (not used because render pass uses LOAD_OP_LOAD)
    std::array<VkClearValue, 2> ui_clear_val;
    ui_clear_val[0] = {{{0.0f, 0.0f, 0.0f, 0.0f}}};
    ui_clear_val[1].depthStencil = {1.0f, 0};

    VkRenderPassBeginInfo ui_renderpass_info{};
    ui_renderpass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    ui_renderpass_info.pNext = nullptr;
    ui_renderpass_info.renderPass = m_ui_render_pass;
    ui_renderpass_info.framebuffer = m_ui_swapchain_framebuffers[image_index];
    ui_renderpass_info.renderArea.offset.x = 0;
    ui_renderpass_info.renderArea.offset.y = 0;
    ui_renderpass_info.renderArea.extent.width = m_extent.width;
    ui_renderpass_info.renderArea.extent.height = m_extent.height;
    ui_renderpass_info.clearValueCount =
        2; // Must provide values even with LOAD_OP_LOAD
    ui_renderpass_info.pClearValues = ui_clear_val.data();

    vkCmdBeginRenderPass(command_buffer, &ui_renderpass_info,
                         VK_SUBPASS_CONTENTS_INLINE);

    static bool ui_pass_logged = false;
    if (!ui_pass_logged) {
      spdlog::info(
          "[RendererVk] UI render pass begun: rp={:#x} fb={:#x} extent={}x{}",
          (uint64_t)m_ui_render_pass,
          (uint64_t)m_ui_swapchain_framebuffers[image_index], m_extent.width,
          m_extent.height);
      ui_pass_logged = true;
    }

    // Set up viewport and scissor for UI render pass
    VkViewport ui_viewport{};
    ui_viewport.x = 0.0f;
    ui_viewport.y = 0.0f;
    ui_viewport.width = (float)m_extent.width;
    ui_viewport.height = (float)m_extent.height;
    ui_viewport.minDepth = 0.0f;
    ui_viewport.maxDepth = 1.0f;
    vkCmdSetViewport(command_buffer, 0, 1, &ui_viewport);

    VkRect2D ui_scissor{};
    ui_scissor.offset.x = 0;
    ui_scissor.offset.y = 0;
    ui_scissor.extent.width = m_extent.width;
    ui_scissor.extent.height = m_extent.height;
    vkCmdSetScissor(command_buffer, 0, 1, &ui_scissor);

    m_noesisUI->Render();

    vkCmdEndRenderPass(command_buffer);
  }

  VK_CHECK_RESULT(vkEndCommandBuffer(command_buffer));
}

void RendererVk::draw_frame(const Camera &camera,
                            const std::vector<RenderableInfo> &renderables) {

  /**
   * The semaphore lifecycle for each image goes like this:
   *
   * 1 - vkAcquireNextImageKHR → signals m_available_image_semaphore[frame]
   * when image is available 2 - vkQueueSubmit → waits on
   * m_available_image_semaphore[frame], signals
   * m_finished_render_semaphore[image_index] when rendering completes 3 -
   * vkQueuePresentKHR → waits on m_finished_render_semaphore[image_index],
   * presents image (no signal) Image goes back to swapchain internally
   * (Later) vkAcquireNextImageKHR → signals
   * m_available_image_semaphore[frame] again when that image is free
   */

  // Wait for the GPU to finish rendering frame N before we start frame
  // N+MAX_CONCURRENT_FRAMES (e.g., with MAX_CONCURRENT_FRAMES=2: wait for
  // frame 0 before starting frame 2) This prevents us from overwriting
  // command buffers/uniforms that GPU is still using
  vkWaitForFences(m_device, 1, &m_in_flight_fences[m_current_frame], VK_TRUE,
                  UINT64_MAX);

  // Get the next available image from the swapchain to render into
  // The presentation engine signals available_image_semaphore when image is
  // ready Note: image_index may not match m_current_frame (e.g., could be
  // 0,1,0,2,1...)
  uint32_t image_index;
  VkResult result =
      vkAcquireNextImageKHR(m_device, m_swapchain, UINT64_MAX,
                            m_available_image_semaphores[m_current_frame],
                            VK_NULL_HANDLE, &image_index);

  if (result == VK_ERROR_OUT_OF_DATE_KHR) {
    recreate_swapchain_and_depth_stencil();
    return;
  } else {
    VK_CHECK_RESULT(result);
  }

  // CHANGED: Defensive bounds check — catch swapchain recreation timing issues
  assert(image_index < m_finished_render_semaphores.size() &&
         "image_index exceeds m_finished_render_semaphores — "
         "swapchain image count mismatch");

  // UPDATE RESOURCES (now safe because we waited on fence)
  update_uniform_buffer(camera);

  // Reset fence to unsignaled state - GPU will signal it when this frame
  // completes
  vkResetFences(m_device, 1, &m_in_flight_fences[m_current_frame]);

  // Prepare command buffer for recording
  vkResetCommandBuffer(m_cmd_buffers[m_current_frame], 0);
  record_draw_commands(m_cmd_buffers[m_current_frame], image_index,
                       renderables);

  // Prepare rendering work to submit gpu
  VkSubmitInfo submit_info{};
  submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

  // GPU WAIT: Don't start rendering until the swapchain image is available
  VkSemaphore wait_semaphores[] = {
      m_available_image_semaphores[m_current_frame]};
  VkPipelineStageFlags wait_stages[] = {
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT}; // Wait before writing
                                                      // colors
  submit_info.waitSemaphoreCount = 1;
  submit_info.pWaitSemaphores = wait_semaphores;
  submit_info.pWaitDstStageMask = wait_stages;

  submit_info.commandBufferCount = 1;
  submit_info.pCommandBuffers = &m_cmd_buffers[m_current_frame];

  // GPU SIGNAL: When rendering finishes, signal this image's render finished
  // semaphore IMPORTANT: Indexed by image_index (not m_current_frame) because
  // semaphores are tied to swapchain images, and same image could be rendered
  // multiple times
  VkSemaphore signal_semaphores[] = {m_finished_render_semaphores[image_index]};
  submit_info.signalSemaphoreCount = 1;
  submit_info.pSignalSemaphores = signal_semaphores;

  // Submit work to GPU queue
  // - Waits on: available_image_semaphore (image ready)
  // - Signals: finished_render_semaphore (rendering done)
  // - Signals: in_flight_fence (entire frame done, CPU can reuse resources)
  VK_CHECK_RESULT(vkQueueSubmit(m_graphics_queue, 1, &submit_info,
                                m_in_flight_fences[m_current_frame]));

  VkPresentInfoKHR present_info{};
  present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

  // PRESENTATION WAIT: Don't display image until rendering is complete
  present_info.waitSemaphoreCount = 1;
  present_info.pWaitSemaphores = signal_semaphores; // Same as submit's signal

  VkSwapchainKHR swapchains[] = {m_swapchain};
  present_info.swapchainCount = 1;
  present_info.pSwapchains = swapchains;

  present_info.pImageIndices = &image_index;

  result = vkQueuePresentKHR(m_present_queue, &present_info);
  if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR ||
      m_input_manager.resize_pending()) {
    recreate_swapchain_and_depth_stencil();
  } else {
    VK_CHECK_RESULT(result);
  }

  // Move to next frame (0 -> 1 -> 0 -> 1 ...)
  m_current_frame = (m_current_frame + 1) % MAX_CONCURRENT_FRAMES;
  m_frameCounter++;
}

VkDescriptorSetLayout RendererVk::create_descriptor_set_layout(
    const std::vector<VkDescriptorSetLayoutBinding> &layout_bindings) {
  // Create descriptor set layout supporting bindless textures
  // Enables dynamic texture indexing without rebinding descriptors

  VkDescriptorSetLayoutBindingFlagsCreateInfo binding_flags{
      VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO};

  // Binding flags enable bindless texturing:
  // - PARTIALLY_BOUND_BIT: Not all descriptors in array must be valid
  // - UPDATE_AFTER_BIND_BIT: GPU can use descriptors while CPU updates them
  VkDescriptorBindingFlags flags = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT |
                                   VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;
  binding_flags.bindingCount = 1;
  binding_flags.pBindingFlags = &flags;

  VkDescriptorSetLayoutCreateInfo layout_info{};
  layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  layout_info.bindingCount = static_cast<uint32_t>(layout_bindings.size());
  layout_info.pBindings = layout_bindings.data();
  layout_info.flags =
      VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
  layout_info.pNext = &binding_flags;

  VkDescriptorSetLayout layout;
  VK_CHECK_RESULT(
      vkCreateDescriptorSetLayout(m_device, &layout_info, nullptr, &layout));
  return layout;
}

VkPipelineLayout RendererVk::create_pipeline_layout(
    VkDevice device, VkDescriptorSetLayout descriptor_set_layout) {
  // Push constant: uint useTexture for per-mesh material differentiation
  VkPushConstantRange push_range{};
  push_range.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
  push_range.offset = 0;
  push_range.size = sizeof(uint32_t);

  VkPipelineLayoutCreateInfo pipeline_layout_info{};
  pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pipeline_layout_info.pNext = nullptr;
  pipeline_layout_info.setLayoutCount = 1;
  pipeline_layout_info.pSetLayouts = &descriptor_set_layout;
  pipeline_layout_info.pushConstantRangeCount = 1;
  pipeline_layout_info.pPushConstantRanges = &push_range;

  VkPipelineLayout pipeline_layout{};
  VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipeline_layout_info, nullptr,
                                         &pipeline_layout));
  return pipeline_layout;
}

UniformBuffer RendererVk::create_uniform_buffer(VmaAllocator allocator,
                                                VkDeviceSize buffer_size) {
  UniformBuffer uniform_buffer;
  VkBufferCreateInfo buffer_info{};
  buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  buffer_info.size = buffer_size;
  buffer_info.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
  buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  VmaAllocationCreateInfo alloc_info{};
  alloc_info.usage =
      VMA_MEMORY_USAGE_CPU_TO_GPU; // Host-visible and coherent by default

  uniform_buffer.allocated_buffer = ToolsVk::create_buffer(
      allocator, buffer_size, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
      VMA_MEMORY_USAGE_CPU_TO_GPU, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

  // Map once for persistent updates
  VK_CHECK_RESULT(
      vmaMapMemory(allocator, uniform_buffer.allocated_buffer.allocation,
                   reinterpret_cast<void **>(&uniform_buffer.mapped)));

  return uniform_buffer;
}

void RendererVk::update_uniform_buffer(const Camera &camera) {
  // static auto startTime = std::chrono::high_resolution_clock::now();

  // auto currentTime = std::chrono::high_resolution_clock::now();
  // float time = std::chrono::duration<float,
  // std::chrono::seconds::period>(currentTime - startTime).count(); glm::vec3
  // start{ 0.0f, 3.0f, -2.0f }; glm::vec3 end{ 0.0f, 3.0f, -5.0f }; float t =
  // sin(time / 4.0f * glm::pi<float>()) * 0.5f + 0.5f;

  // glm::vec3 camera_pos = (1.0f - t) * start + t * end;

  MVP_uniform_object ubo{};
  // ubo.model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f),
  // glm::vec3(0.0f, 1.0f, 0.0f));
  ubo.model = glm::mat4(1.0f);
  ubo.view = glm::lookAt(camera.get_position(),
                         camera.get_position() + camera.get_forward_dir(),
                         glm::vec3(0.0f, 1.0f, 0.0f));
  ubo.projection = glm::perspective(
      glm::radians(45.0f), static_cast<float>(m_extent.width) / m_extent.height,
      0.1f, 1000.0f);

  ubo.projection[1][1] *= -1;

  memcpy(m_uniform_buffers[m_current_frame].mapped, &ubo, sizeof(ubo));
}

void RendererVk::update(uint64_t delta_t) {
  m_totalTimeSeconds += delta_t / 1000.0;

  // Check if shader files have changed, if so, create a new pipeline
  // This allow for hot reloading of shaders while the app is running!
  const bool frag_shader_changed = m_frag_shader_watcher->check_for_changes();
  const bool vert_shader_changed = m_vert_shader_watcher->check_for_changes();
  if (frag_shader_changed || vert_shader_changed) {
    vkDeviceWaitIdle(m_device);
    vkDestroyPipeline(m_device, m_pipeline, nullptr);

    m_pipeline = create_pipeline(m_device, m_render_pass, m_pipeline_layout);
  }
}

void RendererVk::upload_pending_assets(
    const std::vector<RenderableInfo> &pending_renderables) {
  auto &mesh_mgr = MeshManager::Instance();
  auto &mat_mgr = MaterialManager::Instance();

  for (const auto &info : pending_renderables) {
    const auto mesh_opt = mesh_mgr.get_mesh(info.mesh);

    if (!mesh_opt.has_value()) {
      spdlog::error("[RendererVk] Mesh not found: {}", info.mesh.mesh_id);
      continue;
    }

    const auto mat_opt = mat_mgr.get_material(info.material);
    if (!mat_opt.has_value()) {
      spdlog::error("[RendererVk] Material not found: {}",
                    info.material.material_id);
      continue;
    }

    m_resource_manager->upload_mesh_to_gpu(mesh_opt.value());
    m_resource_manager->upload_material_to_gpu(mat_opt.value());
  }
}

void RendererVk::recreate_swapchain_and_depth_stencil() {
  vkDeviceWaitIdle(m_device);

  // CHANGED: Destroy old per-image semaphores — count may change with new
  // swapchain
  for (auto &sem : m_finished_render_semaphores) {
    vkDestroySemaphore(m_device, sem, nullptr);
  }
  m_finished_render_semaphores.clear();

  cleanup_swapchain_and_depth_stencil();

  // Query surface capabilities and clamp pending extent
  VkSurfaceCapabilitiesKHR capabilities;
  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_physical_device, m_surface,
                                            &capabilities);
  m_pending_extent.width = std::max(
      capabilities.minImageExtent.width,
      std::min(m_pending_extent.width, capabilities.maxImageExtent.width));
  m_pending_extent.height = std::max(
      capabilities.minImageExtent.height,
      std::min(m_pending_extent.height, capabilities.maxImageExtent.height));

  m_extent = m_pending_extent;

  create_swapchain();

  m_swapchain_image_views.resize(m_swapchain_images.size());
  for (auto i = 0; i < m_swapchain_images.size(); i++) {
    m_swapchain_image_views[i] = create_swapchain_and_image_views(
        m_device, m_swapchain_images[i], m_swapchain_image_format,
        VK_IMAGE_ASPECT_COLOR_BIT);
  }

  // Recreate depth stencil (destroyed in cleanup_swapchain)
  m_depth_stencil =
      TextureVk::create_depth_stencil(m_physical_device, m_device, m_cmd_pool,
                                      m_graphics_queue, m_allocator, m_extent);

  m_swapchain_framebuffers.resize(m_swapchain_image_views.size());
  for (auto i = 0; i < m_swapchain_image_views.size(); i++) {
    m_swapchain_framebuffers[i] =
        create_framebuffer(m_device, m_render_pass, m_swapchain_image_views[i],
                           m_depth_stencil.view);
  }

  // Recreate UI framebuffers
  m_ui_swapchain_framebuffers.resize(m_swapchain_image_views.size());
  for (auto i = 0; i < m_swapchain_image_views.size(); i++) {
    m_ui_swapchain_framebuffers[i] =
        create_framebuffer(m_device, m_ui_render_pass,
                           m_swapchain_image_views[i], m_depth_stencil.view);
  }

  // CHANGED: Recreate per-image semaphores to match new swapchain image count
  m_finished_render_semaphores.resize(m_swapchain_images.size());
  VkSemaphoreCreateInfo semaphore_info{};
  semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
  for (size_t i = 0; i < m_swapchain_images.size(); i++) {
    VK_CHECK_RESULT(vkCreateSemaphore(m_device, &semaphore_info, nullptr,
                                      &m_finished_render_semaphores[i]));
  }

  if (m_noesisUI) {
    m_noesisUI->SetSize(m_extent.width, m_extent.height);
  }
  m_window_resize_is_pending = false;
}

void RendererVk::OnWindowResize(glm::uvec2 new_dims) {
  m_pending_extent = {new_dims.x, new_dims.y};
  m_window_resize_is_pending = true;
}
} // namespace Expectre