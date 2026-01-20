// Library macros
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#define STB_IMAGE_IMPLEMENTATION // includes stb function bodies

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
#include "Mesh.h"
#include "Model.h"
#include "RenderContextVk.h"
#include "ShaderFileWatcher.h"
#include "ToolsVk.h"

#include "scene/Camera.h"

struct MVP_uniform_object {
  glm::mat4 model;
  glm::mat4 view;
  glm::mat4 projection;
};

namespace Expectre {

RendererVk::RendererVk(VkPhysicalDevice &physical_device, VkDevice &device,
                       VmaAllocator &allocator, VkSurfaceKHR &surface,
                       VkQueue &graphics_queue, uint32_t &graphics_queue_index,
                       VkQueue &present_queue, uint32_t &present_queue_index)
    : m_physical_device{physical_device}, m_device{device},
      m_allocator{allocator}, m_surface{surface},
      m_graphics_queue{graphics_queue},
      m_graphics_queue_index{graphics_queue_index},
      m_present_queue{present_queue},
      m_present_queue_index{present_queue_index},
      m_extent{RESOLUTION_X, RESOLUTION_Y} {
  /*	const auto& phys_device = context.get_phys_device();
          const auto& device = context.get_device();
          const auto& allocator = context.get_allocator();*/

  // Command buffers and swapchain
  create_swapchain();

  m_swapchain_image_views.resize(m_swapchain_images.size());
  for (auto i = 0; i < m_swapchain_images.size(); i++) {
    m_swapchain_image_views[i] = create_swapchain_image_views(
        device, m_swapchain_images[i], m_swapchain_image_format,
        VK_IMAGE_ASPECT_COLOR_BIT);
  }
  m_cmd_pool = create_command_pool(device, graphics_queue_index);
  m_depth_stencil =
      TextureVk::create_depth_stencil(m_physical_device, device, m_cmd_pool,
                                      m_graphics_queue, allocator, m_extent);
  m_render_pass = create_renderpass(device, m_swapchain_image_format,
                                    m_depth_stencil.image_info.format, true);

  VkDescriptorSetLayoutBinding ubo_layout_binding{};
  ubo_layout_binding.binding = 0;
  ubo_layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  ubo_layout_binding.descriptorCount = 1;
  ubo_layout_binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
  ubo_layout_binding.pImmutableSamplers = nullptr;

  VkDescriptorSetLayoutBinding sampler_layout_binding{};
  sampler_layout_binding.binding = 1;
  sampler_layout_binding.descriptorCount = 1;
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
    VkImageView view = m_swapchain_image_views[i];
    m_swapchain_framebuffers[i] = create_framebuffer(
        device, m_swapchain_image_views[i], m_depth_stencil.view);
  }
  // std::ignore = create_texture_from_file(WORKSPACE_DIR +
  // std::string("/assets/teapot/brick.png"));
  m_texture = TextureVk::create_texture_from_file(
      device, m_cmd_pool, m_graphics_queue, allocator,
      WORKSPACE_DIR + std::string("/assets/teapot/brick.png"));
  m_texture_sampler =
      ToolsVk::create_texture_sampler(m_physical_device, device);
  // m_models.push_back(
  //     load_model(WORKSPACE_DIR + std::string("/assets/teapot/teapot.obj")));
  // m_models.push_back(
  //     load_model(WORKSPACE_DIR + std::string("/assets/bunny.obj")));
  create_geometry_buffer();

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

  m_ready = true;
}

void RendererVk::cleanup_swapchain() {
  // Destroy depth buffer
  vkDestroyImageView(m_device, m_depth_stencil.view, nullptr);
  vmaDestroyImage(m_allocator, m_depth_stencil.image,
                  m_depth_stencil.allocation);

  for (auto framebuffer : m_swapchain_framebuffers) {
    vkDestroyFramebuffer(m_device, framebuffer, nullptr);
  }

  for (auto imageView : m_swapchain_image_views) {
    vkDestroyImageView(m_device, imageView, nullptr);
  }

  vkDestroySwapchainKHR(m_device, m_swapchain, nullptr);
}

RendererVk::~RendererVk() {

  vkDeviceWaitIdle(m_device);

  // Destroy synchronization objects
  for (size_t i = 0; i < MAX_CONCURRENT_FRAMES; ++i) {
    vkDestroySemaphore(m_device, m_available_image_semaphores[i], nullptr);
    vkDestroySemaphore(m_device, m_finished_render_semaphores[i], nullptr);
    vkDestroyFence(m_device, m_in_flight_fences[i], nullptr);
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

  vmaDestroyBuffer(m_allocator, m_geometry_buffer.buffer,
                   m_geometry_buffer.allocation);

  // Destroy pipeline and related layouts
  vkDestroyPipeline(m_device, m_pipeline, nullptr);
  vkDestroyPipelineLayout(m_device, m_pipeline_layout, nullptr);
  vkDestroyRenderPass(m_device, m_render_pass, nullptr);

  // Destroy texture resources
  vkDestroySampler(m_device, m_texture_sampler, nullptr);
  vkDestroyImageView(m_device, m_texture.view, nullptr);
  vkDestroyImage(m_device, m_texture.image, nullptr);
  vmaFreeMemory(m_allocator, m_texture.allocation);

  // Destroy command pool
  vkDestroyCommandPool(m_device, m_cmd_pool, nullptr);

  cleanup_swapchain();

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
  // VkExtent2D extent =
  // tools::choose_swap_extent(swapchain_support_details.capabilities,
  // m_window);

  // uint32_t image_count = swapchain_support_details.capabilities.minImageCount
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
  create_info.imageExtent = m_extent;
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

VkImageView RendererVk::create_swapchain_image_views(VkDevice device,
                                                     VkImage image,
                                                     VkFormat format,
                                                     VkImageAspectFlags flags) {
  VkImageView image_view;

  image_view = ToolsVk::create_image_view(device, image,
                                          m_swapchain_image_format, flags);

  return image_view;
}

void RendererVk::create_geometry_buffer() {

  static uint32_t geometry_buffer_init_size_bytes = 1024 * 1024 * 64; // 64MB
  // === STAGING BUFFERS ===

  auto buffer = ToolsVk::create_buffer(
      m_allocator, geometry_buffer_init_size_bytes,
      VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
          VK_BUFFER_USAGE_TRANSFER_DST_BIT,
      VMA_MEMORY_USAGE_GPU_ONLY, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

  m_geometry_buffer.allocation = buffer.allocation;
  m_geometry_buffer.buffer = buffer.buffer;
  m_geometry_buffer.vertex_offset = 0;
  m_geometry_buffer.index_begin =
      static_cast<uint32_t>(geometry_buffer_init_size_bytes * 0.80);
  m_geometry_buffer.index_offset = m_geometry_buffer.index_begin;
  m_geometry_buffer.buffer_size = geometry_buffer_init_size_bytes;
}

VkRenderPass RendererVk::create_renderpass(VkDevice device,
                                           VkFormat color_format,
                                           VkFormat depth_format,
                                           bool is_presenting_pass) {
  VkAttachmentDescription color{};
  color.format = color_format;
  color.samples = VK_SAMPLE_COUNT_1_BIT;
  color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  color.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  color.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  color.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  color.finalLayout = is_presenting_pass
                          ? VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
                          : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

  VkAttachmentDescription depth{};
  depth.format = depth_format;
  depth.samples = VK_SAMPLE_COUNT_1_BIT;
  depth.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  depth.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  depth.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  depth.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  depth.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  depth.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

  VkAttachmentReference colorRef{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
  VkAttachmentReference depthRef{
      1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};

  VkSubpassDescription sub{};
  sub.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  sub.colorAttachmentCount = 1;
  sub.pColorAttachments = &colorRef;
  sub.pDepthStencilAttachment =
      (depth_format != VK_FORMAT_UNDEFINED) ? &depthRef : nullptr;

  std::vector<VkAttachmentDescription> atts = {color};
  if (depth_format != VK_FORMAT_UNDEFINED)
    atts.push_back(depth);

  VkRenderPass render_pass;
  VkRenderPassCreateInfo rpci{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
  rpci.attachmentCount = (uint32_t)atts.size();
  rpci.pAttachments = atts.data();
  rpci.subpassCount = 1;
  rpci.pSubpasses = &sub;
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

VkFramebuffer RendererVk::create_framebuffer(VkDevice device, VkImageView view,
                                             VkImageView depth_view) {
  VkResult err;
  VkFramebuffer framebuffer;
  std::vector<VkImageView> attachments{view};

  if (depth_view != VK_NULL_HANDLE) {
    attachments.push_back(depth_view);
  }

  // All framebuffers use same renderpass setup
  VkFramebufferCreateInfo framebuffer_info{};
  framebuffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
  framebuffer_info.renderPass = m_render_pass;
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
  m_available_image_semaphores.resize(MAX_CONCURRENT_FRAMES);
  m_finished_render_semaphores.resize(MAX_CONCURRENT_FRAMES);
  m_in_flight_fences.resize(MAX_CONCURRENT_FRAMES);

  VkSemaphoreCreateInfo semaphore_info{};
  semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

  VkFenceCreateInfo fence_info{};
  fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

  for (auto i = 0; i < MAX_CONCURRENT_FRAMES; i++) {
    // For each swapchain image
    // Create an "available" and "finished" semphore
    // Create fence as well
    auto avail = vkCreateSemaphore(m_device, &semaphore_info, nullptr,
                                   &m_available_image_semaphores[i]);
    auto finished = vkCreateSemaphore(m_device, &semaphore_info, nullptr,
                                      &m_finished_render_semaphores[i]);
    auto fences =
        vkCreateFence(m_device, &fence_info, nullptr, &m_in_flight_fences[i]);

    VK_CHECK_RESULT(avail);
    VK_CHECK_RESULT(finished);
    VK_CHECK_RESULT(fences);
  }
}

void RendererVk::record_draw_commands(VkCommandBuffer command_buffer,
                                      uint32_t image_index) {
  VkCommandBufferBeginInfo begin_info{};
  begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  begin_info.pNext = nullptr;
  begin_info.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
  begin_info.pInheritanceInfo = nullptr;

  VK_CHECK_RESULT(vkBeginCommandBuffer(command_buffer, &begin_info));

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
  vkCmdBindVertexBuffers(command_buffer, 0, 1, &m_geometry_buffer.buffer,
                         offsets);

  vkCmdBindIndexBuffer(command_buffer, m_geometry_buffer.buffer,
                       m_geometry_buffer.index_begin, VK_INDEX_TYPE_UINT32);

  vkCmdBindDescriptorSets(
      command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline_layout, 0, 1,
      &m_uniform_buffers[m_current_frame].descriptorSet, 0, nullptr);

  // Calculate index count: (bytes used) / (bytes per index)
  auto index_count =
      (m_geometry_buffer.index_offset - m_geometry_buffer.index_begin) /
      sizeof(uint32_t);
  vkCmdDrawIndexed(command_buffer, index_count, 1, 0, 0, 0);

  vkCmdEndRenderPass(command_buffer);

  VK_CHECK_RESULT(vkEndCommandBuffer(command_buffer));
}

void RendererVk::draw_frame(const Camera &camera) {
  vkWaitForFences(m_device, 1, &m_in_flight_fences[m_current_frame], VK_TRUE,
                  UINT64_MAX);

  uint32_t image_index;
  VkResult result =
      vkAcquireNextImageKHR(m_device, m_swapchain, UINT64_MAX,
                            m_available_image_semaphores[m_current_frame],
                            VK_NULL_HANDLE, &image_index);
  VK_CHECK_RESULT(result);
  update_uniform_buffer(camera);
  vkResetFences(m_device, 1, &m_in_flight_fences[m_current_frame]);
  vkResetCommandBuffer(m_cmd_buffers[m_current_frame], 0);
  record_draw_commands(m_cmd_buffers[m_current_frame], image_index);

  VkSubmitInfo submit_info{};
  submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

  VkSemaphore wait_semaphores[] = {
      m_available_image_semaphores[m_current_frame]};
  VkPipelineStageFlags wait_stages[] = {
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
  submit_info.waitSemaphoreCount = 1;
  submit_info.pWaitSemaphores = wait_semaphores;
  submit_info.pWaitDstStageMask = wait_stages;

  submit_info.commandBufferCount = 1;
  submit_info.pCommandBuffers = &m_cmd_buffers[m_current_frame];

  VkSemaphore signal_semaphores[] = {
      m_finished_render_semaphores[m_current_frame]};
  submit_info.signalSemaphoreCount = 1;
  submit_info.pSignalSemaphores = signal_semaphores;

  VK_CHECK_RESULT(vkQueueSubmit(m_graphics_queue, 1, &submit_info,
                                m_in_flight_fences[m_current_frame]));

  VkPresentInfoKHR present_info{};
  present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

  present_info.waitSemaphoreCount = 1;
  present_info.pWaitSemaphores = signal_semaphores;

  VkSwapchainKHR swapchains[] = {m_swapchain};
  present_info.swapchainCount = 1;
  present_info.pSwapchains = swapchains;

  present_info.pImageIndices = &image_index;
  result = vkQueuePresentKHR(m_present_queue, &present_info);
  VK_CHECK_RESULT(result);
  m_current_frame = (m_current_frame + 1) % MAX_CONCURRENT_FRAMES;
}

VkDescriptorSetLayout RendererVk::create_descriptor_set_layout(
    const std::vector<VkDescriptorSetLayoutBinding> &layout_bindings) {
  VkDescriptorSetLayoutCreateInfo layout_info{};
  layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  layout_info.bindingCount = static_cast<uint32_t>(layout_bindings.size());
  layout_info.pBindings = layout_bindings.data();
  VkDescriptorSetLayout layout;
  VK_CHECK_RESULT(
      vkCreateDescriptorSetLayout(m_device, &layout_info, nullptr, &layout));
  return layout;
}

VkPipelineLayout RendererVk::create_pipeline_layout(
    VkDevice device, VkDescriptorSetLayout descriptor_set_layout) {
  VkPipelineLayoutCreateInfo pipeline_layout_info{};
  pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pipeline_layout_info.pNext = nullptr;
  pipeline_layout_info.setLayoutCount = 1;
  // pipeline_layout_info.pushConstantRangeCount = 0;
  pipeline_layout_info.pSetLayouts = &descriptor_set_layout;

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
  ubo.view = glm::lookAt(camera.get_position(), camera.get_forward_dir(),
                         glm::vec3(0.0f, 1.0f, 0.0f));
  ubo.projection = glm::perspective(
      glm::radians(45.0f), static_cast<float>(m_extent.width) / m_extent.height,
      0.1f, 1000.0f);

  ubo.projection[1][1] *= -1;

  memcpy(m_uniform_buffers[m_current_frame].mapped, &ubo, sizeof(ubo));
}

void RendererVk::update(uint64_t delta_t) {

  // update camera velocity
  glm::vec3 dir(0.0f);

  if (m_camera.moveForward)
    dir += glm::vec3(0.0f, 0.0f, -1.0f);
  if (m_camera.moveBack)
    dir += glm::vec3(0.0f, 0.0f, 1.0f);
  if (m_camera.moveLeft)
    dir += glm::vec3(-1.0f, 0.0f, 0.0f);
  if (m_camera.moveRight)
    dir += glm::vec3(1.0f, 0.0f, 0.0f);

  if (glm::length(dir) > 0.0f)
    dir = glm::normalize(dir);

  m_camera.pos +=
      dir * m_camera.camera_speed * static_cast<float>(delta_t) / 1000.0f;

  // Check if shader files have changed
  const bool frag_shader_changed = m_frag_shader_watcher->check_for_changes();
  const bool vert_shader_changed = m_vert_shader_watcher->check_for_changes();
  if (frag_shader_changed || vert_shader_changed) {
    m_pipeline = create_pipeline(m_device, m_render_pass, m_pipeline_layout);
  }

  // if (m_ui_renderer) {
  //	m_ui_renderer->Update(Time::Instance().RunningTimeSeconds());
  // }
}

void RendererVk::upload_mesh_to_gpu(const Mesh &mesh) {
  // Early return if mesh is empty
  if (mesh.vertices.empty() || mesh.indices.empty()) {
    spdlog::warn("Attempted to upload empty mesh to GPU");
    return;
  }

  VkDeviceSize vertex_buffer_size = sizeof(Vertex) * mesh.vertices.size();
  VkDeviceSize index_buffer_size = sizeof(uint32_t) * mesh.indices.size();
  
  // Align vertex buffer size to 4 bytes to keep index offsets aligned
  vertex_buffer_size = (vertex_buffer_size + 3) & ~3;

  VkDeviceSize mesh_buffer_size = vertex_buffer_size + index_buffer_size;

  // === STAGING BUFFERS ===
  AllocatedBuffer mesh_staging = ToolsVk::create_buffer(
      m_allocator, mesh_buffer_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
      VMA_MEMORY_USAGE_CPU_ONLY,
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
          VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

  void *data{nullptr};
  VK_CHECK_RESULT(vmaMapMemory(m_allocator, mesh_staging.allocation, &data));
  uint8_t *start_ptr = static_cast<uint8_t *>(data);
  memcpy(start_ptr, mesh.vertices.data(), vertex_buffer_size);
  memcpy(start_ptr + vertex_buffer_size, mesh.indices.data(),
         index_buffer_size);

  // === copy from staging to device buffer

  VkBufferCopy vertex_region{};
  vertex_region.srcOffset = 0;
  vertex_region.dstOffset =
      static_cast<VkDeviceSize>(m_geometry_buffer.vertex_offset);
  vertex_region.size = vertex_buffer_size;

  // Verify this won't write over our index buffer
  assert(m_geometry_buffer.vertex_offset + vertex_buffer_size <=
         m_geometry_buffer.index_begin);

  // first, copy vertices
  ToolsVk::copy_buffer(m_device, m_cmd_pool, m_graphics_queue,
                       mesh_staging.buffer, m_geometry_buffer.buffer,
                       vertex_region);
  m_geometry_buffer.vertex_offset += vertex_buffer_size;

  VkBufferCopy index_region{};
  index_region.srcOffset = static_cast<VkDeviceSize>(vertex_buffer_size);
  index_region.dstOffset =
      static_cast<VkDeviceSize>(m_geometry_buffer.index_offset);
  index_region.size = index_buffer_size;

  // Verify this won't write outside the geometry buffer
  assert(m_geometry_buffer.index_offset + index_buffer_size <=
         m_geometry_buffer.buffer_size);
  // second, copy indices
  ToolsVk::copy_buffer(m_device, m_cmd_pool, m_graphics_queue,
                       mesh_staging.buffer, m_geometry_buffer.buffer,
                       index_region);
  m_geometry_buffer.index_offset += index_buffer_size;

  // todo - add mesh tracking logic (keep track of where meshes are in the geoemetry buffer)

  // Cleanup staging
  vmaUnmapMemory(m_allocator, mesh_staging.allocation);
  vmaDestroyBuffer(m_allocator, mesh_staging.buffer, mesh_staging.allocation);
}
} // namespace Expectre