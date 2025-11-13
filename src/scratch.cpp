
		/// From RenderDevice
		//@{
		const Noesis::DeviceCaps& GetCaps() const override;
		Noesis::Ptr<Noesis::RenderTarget> CreateRenderTarget(const char* label, uint32_t width,
			uint32_t height, uint32_t sampleCount, bool needsStencil) override;
		Noesis::Ptr<Noesis::RenderTarget> CloneRenderTarget(const char* label,
			Noesis::RenderTarget* surface) override;
		Noesis::Ptr<Noesis::Texture> CreateTexture(const char* label, uint32_t width, uint32_t height,
			uint32_t numLevels, Noesis::TextureFormat::Enum format, const void** data) override;
		void UpdateTexture(Noesis::Texture* texture, uint32_t level, uint32_t x, uint32_t y,
			uint32_t width, uint32_t height, const void* data) override;
		void BeginOffscreenRender() override;
		void EndOffscreenRender() override;
		void BeginOnscreenRender() override;
		void EndOnscreenRender() override;
		void SetRenderTarget(Noesis::RenderTarget* surface) override;
		void BeginTile(Noesis::RenderTarget* surface, const Noesis::Tile& tile) override;
		void EndTile(Noesis::RenderTarget* surface) override;
		void ResolveRenderTarget(Noesis::RenderTarget* surface, const Noesis::Tile* tiles,
			uint32_t numTiles) override;
		void* MapVertices(uint32_t bytes) override;
		void UnmapVertices() override;
		void* MapIndices(uint32_t bytes) override;
		void UnmapIndices() override;
		void DrawBatch(const Noesis::Batch& batch) override;
		//@}

	
    
    const Noesis::DeviceCaps& RendererVk::GetCaps() const
	{
		static Noesis::DeviceCaps caps = {};

		// linearRendering: true if swapchain is SRGB format
		caps.linearRendering = (m_swapchain_image_format == VK_FORMAT_R8G8B8A8_SRGB ||
			m_swapchain_image_format == VK_FORMAT_B8G8R8A8_SRGB);
		caps.clipSpaceYInverted = false;
		caps.centerPixelOffset = 0.0;

		return caps;
	}

	class VKTexture : public Noesis::Texture
	{
	public:
		VkImage image;
		VkImageView view;
		VmaAllocation allocation;
		VkImageCreateInfo image_info;
		VkImageViewCreateInfo view_info;
		VkImageLayout layout;
		VKTexture() {}

		~VKTexture()
		{

		}

		uint32_t GetWidth() const override { return image_info.extent.width; }
		uint32_t GetHeight() const override { return image_info.extent.height; }
		bool HasMipMaps() const override { return image_info.mipLevels > 1; }
		bool IsInverted() const override { return false; }
		bool HasAlpha() const override { return true; }


		//VKRenderDevice* device = VK_NULL_HANDLE;

		//VkFormat format = VK_FORMAT_R8G8B8A8_SRGB;
		//VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;

	};

	//Noesis::Ptr<Noesis::Texture> RendererVk::WrapTexture(VkImage image, uint32_t width, uint32_t height,
	//	uint32_t levels, VkFormat format, VkImageLayout layout, bool isInverted, bool hasAlpha)
	//{
	//	Noesis::Ptr<VKTexture> texture = Noesis::MakePtr<VKTexture>();

	//	texture->image = image;
	//	texture->format = format;
	//	texture->layout = layout;
	//	texture->image_info.extent = { width, height, 1 };
	//	texture->image_info.mipLevels = levels;
	//	return texture;
	//}


	class VKRenderTarget final : public Noesis::RenderTarget
	{
	public:
		~VKRenderTarget()
		{
		}

		Noesis::Texture* GetTexture() override { return color; }

		Noesis::Ptr<VKTexture> color;
		Noesis::Ptr<VKTexture> colorAA;
		Noesis::Ptr<VKTexture> stencil;

		VkFramebuffer framebuffer = VK_NULL_HANDLE;
		VkRenderPass renderPass = VK_NULL_HANDLE;
		VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT;
	};


	Noesis::Ptr<Noesis::RenderTarget> RendererVk::CreateRenderTarget(const char* label, uint32_t width, uint32_t height, uint32_t sampleCount, bool needsStencil)
	{
		spdlog::info("CreateRenderTarget()");
		using Noesis::MakePtr;

		auto surface = MakePtr<VKRenderTarget>();
		surface->samples = VK_SAMPLE_COUNT_1_BIT;

		const VkFormat colorFmt = m_swapchain_image_format;

		// 1) Single-sample color (attachment + sampled)
		auto colorTex = TextureVk::create_texture(
			m_device, m_cmd_pool, m_graphics_queue, m_allocator,
			/*pixelData*/ nullptr,
			width, height,
			/*mip_levels*/ 1,
			/*format*/ colorFmt,
			/*aspect*/ VK_IMAGE_ASPECT_COLOR_BIT,
			/*extra_usage*/ VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
			/*final_layout*/ VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

		auto color = Noesis::MakePtr<VKTexture>();
		color->image = colorTex.image;
		color->view = colorTex.view;
		color->allocation = colorTex.allocation;
		color->image_info = colorTex.image_info;
		color->view_info = colorTex.view_info;
		color->layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		color->image_info.extent = { width, height };
		surface->color = color;  // This is the one Noesis will sample

		// 2) Optional depth/stencil
		std::vector<VkImageView> atts;
		atts.reserve(2);
		atts.push_back(surface->color->view);

		VkFormat dsFmt = VK_FORMAT_UNDEFINED;
		if (needsStencil) {
			dsFmt = ToolsVk::find_supported_format(
				m_chosen_phys_device,
				{ VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D32_SFLOAT_S8_UINT },
				VK_IMAGE_TILING_OPTIMAL,
				VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);

			auto dsTex = TextureVk::create_texture(
				m_device, m_cmd_pool, m_graphics_queue, m_allocator,
				/*pixelData*/ nullptr,
				width, height,
				/*mip_levels*/ 1,
				/*format*/ dsFmt,
				/*aspect*/ VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT,
				/*extra_usage*/ VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
				/*final_layout*/ VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

			auto ds = Noesis::MakePtr<VKTexture>();
			ds->image = dsTex.image;
			ds->view = dsTex.view;
			ds->allocation = dsTex.allocation;
			ds->image_info = dsTex.image_info;
			ds->view_info = dsTex.view_info;
			ds->layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
			ds->image_info.extent = { width, height };
			surface->stencil = ds;

			atts.push_back(surface->stencil->view);
		}

		// 3) Render pass (single-sample, no resolve)
		VkAttachmentDescription colorAtt{};
		colorAtt.format = colorFmt;
		colorAtt.samples = VK_SAMPLE_COUNT_1_BIT;
		colorAtt.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		colorAtt.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		colorAtt.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		colorAtt.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		colorAtt.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		colorAtt.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		VkAttachmentDescription dsAtt{};
		if (needsStencil) {
			dsAtt.format = dsFmt;
			dsAtt.samples = VK_SAMPLE_COUNT_1_BIT;
			dsAtt.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			dsAtt.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			dsAtt.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			dsAtt.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			dsAtt.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			dsAtt.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		}

		VkAttachmentReference colorRef{ 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
		VkAttachmentReference dsRef{};
		if (needsStencil) {
			dsRef.attachment = 1;
			dsRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		}

		VkSubpassDescription sub{};
		sub.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		sub.colorAttachmentCount = 1;
		sub.pColorAttachments = &colorRef;
		sub.pDepthStencilAttachment = needsStencil ? &dsRef : nullptr;

		VkSubpassDependency dep{};
		dep.srcSubpass = VK_SUBPASS_EXTERNAL;
		dep.dstSubpass = 0;
		dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dep.srcAccessMask = 0;
		dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

		VkAttachmentDescription attachments[2];
		uint32_t attachmentCount = 1;
		attachments[0] = colorAtt;
		if (needsStencil) {
			attachments[1] = dsAtt;
			attachmentCount = 2;
		}

		VkRenderPassCreateInfo rpci{ VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
		rpci.attachmentCount = attachmentCount;
		rpci.pAttachments = attachments;
		rpci.subpassCount = 1;
		rpci.pSubpasses = &sub;
		rpci.dependencyCount = 1;
		rpci.pDependencies = &dep;

		VK_CHECK_RESULT(vkCreateRenderPass(m_device, &rpci, nullptr, &surface->renderPass));

		// 4) Framebuffer
		VkFramebufferCreateInfo fbci{ VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
		fbci.renderPass = surface->renderPass;
		fbci.attachmentCount = static_cast<uint32_t>(atts.size());
		fbci.pAttachments = atts.data();
		fbci.width = width;
		fbci.height = height;
		fbci.layers = 1;
		VK_CHECK_RESULT(vkCreateFramebuffer(m_device, &fbci, nullptr, &surface->framebuffer));

		return surface;

	}

	Noesis::Ptr<Noesis::RenderTarget> RendererVk::CloneRenderTarget(const char* label, Noesis::RenderTarget* surface_)
	{
		spdlog::info("CreateRenderTarget()");

		using namespace Noesis;

		VKRenderTarget* src = (VKRenderTarget*)surface_;

		uint32_t width = src->color->GetWidth();
		uint32_t height = src->color->GetHeight();

		Ptr<VKRenderTarget> surface = MakePtr<VKRenderTarget>();
		surface->renderPass = src->renderPass;
		surface->samples = src->samples;
		surface->colorAA = src->colorAA;
		surface->stencil = src->stencil;

		Vector<VkImageView, 3> attachments;
		if (surface->stencil)
		{
			attachments.PushBack(surface->stencil->view);
		}

		//EnsureTransferCommands();

		VkFramebufferCreateInfo framebufferInfo{};
		framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferInfo.renderPass = surface->renderPass;
		framebufferInfo.attachmentCount = attachments.Size();
		framebufferInfo.pAttachments = attachments.Data();
		framebufferInfo.width = width;
		framebufferInfo.height = height;
		framebufferInfo.layers = 1;

		VK_CHECK_RESULT(vkCreateFramebuffer(m_device, &framebufferInfo, nullptr, &surface->framebuffer));
		VK_NAME(surface->framebuffer, FRAMEBUFFER, "Noesis_%s_FrameBuffer", label);
		return surface;
	}


	Noesis::Ptr<Noesis::Texture> RendererVk::CreateTexture(const char* label, uint32_t width, uint32_t height,
		uint32_t numLevels, Noesis::TextureFormat::Enum format, const void** data)
	{
		spdlog::info("NS: createtexture()");
		// covert noesis format to vulkan foramt
		VkFormat vk_format = VK_FORMAT_R8G8B8A8_SRGB;
		switch (format) {
		case Noesis::TextureFormat::R8:
			vk_format = VK_FORMAT_R8_UNORM;
			break;
		case Noesis::TextureFormat::RGBA8:
			vk_format = VK_FORMAT_R8G8B8A8_SRGB;
			break;
		default:
			assert(false);
			break;
		}
		TextureVk tex = TextureVk::create_texture(m_device, m_cmd_pool, m_graphics_queue, m_allocator, data, width, height, numLevels);
		Noesis::Ptr<VKTexture> texture = Noesis::MakePtr<VKTexture>();
		texture->image = tex.image;
		texture->image_info = tex.image_info;
		texture->allocation = tex.allocation;
		texture->view = tex.view;
		texture->view_info = tex.view_info;

		static int i = 0;
		ToolsVk::set_object_name(m_device, (uint64_t)tex.image, VK_OBJECT_TYPE_IMAGE, std::string("ns texture image " + i).c_str());
		ToolsVk::set_object_name(m_device, (uint64_t)tex.view, VK_OBJECT_TYPE_IMAGE_VIEW, std::string("ns texture image view " + i).c_str());
		i++;

		return texture;
	}

	void RendererVk::UpdateTexture(Noesis::Texture* texture_, uint32_t level, uint32_t x, uint32_t y, uint32_t width, uint32_t height, const void* data)
	{
		spdlog::info("NS: updateTexture()");

		VKTexture* texture = (VKTexture*)texture_;

		uint32_t texel_size = texture->image_info.format == VK_FORMAT_R8_UNORM ? 1 : 4;
		uint32_t size = width * height * texel_size;
		AllocatedBuffer staging_buffer = ToolsVk::create_buffer(m_allocator, size,
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VMA_MEMORY_USAGE_CPU_ONLY, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

		// Copy data into staging buffer
		void* mapped;
		VK_CHECK_RESULT(vmaMapMemory(m_allocator, staging_buffer.allocation, &mapped));
		memcpy(mapped, data, static_cast<size_t>(size));
		vmaUnmapMemory(m_allocator, staging_buffer.allocation);

		TextureVk::transition_image_layout(m_device, m_cmd_pool, m_graphics_queue, texture->image, texture->image_info.format, texture->layout, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

		VkBufferImageCopy region{};
		region.imageOffset.x = x;
		region.imageOffset.y = y;
		region.imageExtent.width = width;
		region.imageExtent.height = height;
		region.imageExtent.depth = 1;
		region.imageSubresource.mipLevel = level;
		region.bufferOffset = 0;
		region.imageSubresource.layerCount = 1;
		region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

		VkCommandBuffer cmd = ToolsVk::begin_single_time_commands(m_device, m_cmd_pool);
		vkCmdCopyBufferToImage(cmd, staging_buffer.buffer, texture->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
		ToolsVk::end_single_time_commands(m_device, m_cmd_pool, cmd, m_graphics_queue);

		// Transition back for shader access
		TextureVk::transition_image_layout(m_device, m_cmd_pool, m_graphics_queue,
			texture->image, texture->image_info.format,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			texture->layout);

		vmaDestroyBuffer(m_allocator, staging_buffer.buffer, staging_buffer.allocation);
	}

	void RendererVk::BeginOffscreenRender()
	{
		spdlog::info("NS: BeginOffscreenRender()");

		// NA
	}
	void RendererVk::EndOffscreenRender()
	{
		spdlog::info("NS: EndOffScreenRender()");

		// NA
	}
	void RendererVk::BeginOnscreenRender()
	{
		spdlog::info("NS: BeginOnscreenRender()");

		if (!m_noesis.active_rt) return;
		auto* rt = (VKRenderTarget*)m_noesis.active_rt;
		VkClearValue clears[2]{};
		clears[0].color = { {0,0,0,0} }; uint32_t n = 1;
		if (rt->stencil) { clears[1].depthStencil = { 1.f,0 }; n = 2; }
		VkRenderPassBeginInfo bi{ VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
		bi.renderPass = rt->renderPass; bi.framebuffer = rt->framebuffer;
		bi.renderArea = { {0,0},{rt->color->GetWidth(),rt->color->GetHeight()} };
		bi.clearValueCount = n; bi.pClearValues = clears;
		vkCmdBeginRenderPass(m_cmd_buffers[m_current_frame], &bi, VK_SUBPASS_CONTENTS_INLINE);

	}
	void RendererVk::EndOnscreenRender()
	{
		spdlog::info("NS: EndOnscreenRender()");

		// Close the Noesis pass if one is open
		if (!m_noesis.active_rt) return;
		VkCommandBuffer cmd = m_cmd_buffers[m_current_frame];
		vkCmdEndRenderPass(cmd);
	}
	void RendererVk::SetRenderTarget(Noesis::RenderTarget* surface_)
	{
		spdlog::info("NS: SetRenderTarget()");

		auto* rt = (VKRenderTarget*)surface_;
		m_noesis.active_rt = surface_;
		VkViewport vp{ 0,0,(float)rt->color->GetWidth(),(float)rt->color->GetHeight(),0.f,1.f };
		vkCmdSetViewport(m_cmd_buffers[m_current_frame], 0, 1, &vp);
		VkRect2D sc{ {0,0},{rt->color->GetWidth(),rt->color->GetHeight()} };
		vkCmdSetScissor(m_cmd_buffers[m_current_frame], 0, 1, &sc);

	}

	void RendererVk::BeginTile(Noesis::RenderTarget* surface, const Noesis::Tile& tile)
	{
		spdlog::info("NS: BeginTile()");
	}
	void RendererVk::EndTile(Noesis::RenderTarget* surface)
	{
		spdlog::info("NS: EndTile()");

	}
	void RendererVk::ResolveRenderTarget(Noesis::RenderTarget* surface, const Noesis::Tile* tiles, uint32_t numTiles)
	{
		spdlog::info("NS: ResolveRenderTarget()");

		// Not supporting msaa yet
		// NA
	}
	void* RendererVk::MapVertices(uint32_t bytes)
	{
		spdlog::info("NS: MapVertices()");

		// Toss any previous staging from the last frame
		if (m_vertex_staging_ns.buffer != VK_NULL_HANDLE) {
			vmaDestroyBuffer(m_allocator, m_vertex_staging_ns.buffer, m_vertex_staging_ns.allocation);
			m_vertex_staging_ns = {};
		}

		if (bytes == 0) {
			return nullptr;
		}

		// allocate memory
		m_vertex_staging_ns = ToolsVk::create_buffer(m_allocator, bytes, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VMA_MEMORY_USAGE_CPU_ONLY, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

		// map memory
		void* data;
		if (bytes) {
			VK_CHECK_RESULT(vmaMapMemory(m_allocator, m_vertex_staging_ns.allocation, &data));
		}
		return data;
	}

	void RendererVk::UnmapVertices()
	{
		spdlog::info("NS: UnmapVertices()");

		if (m_vertex_staging_ns.buffer == VK_NULL_HANDLE) return;

		vmaUnmapMemory(m_allocator, m_vertex_staging_ns.allocation);

		// Recreate a tight device-local VBO sized to the written bytes
		if (m_noesis.vertex_buffer.buffer != VK_NULL_HANDLE) {
			vmaDestroyBuffer(m_allocator, m_noesis.vertex_buffer.buffer, m_noesis.vertex_buffer.allocation);
			m_noesis.vertex_buffer = {};
		}

		vmaDestroyBuffer(m_allocator, m_vertex_staging_ns.buffer, m_vertex_staging_ns.allocation);
		m_vertex_staging_ns = {};

	}
	void* RendererVk::MapIndices(uint32_t bytes)
	{
		spdlog::info("NS: MapIndices()");

		// Toss any previous staging from the last frame
		if (m_index_staging_ns.buffer != VK_NULL_HANDLE) {
			vmaDestroyBuffer(m_allocator, m_index_staging_ns.buffer, m_index_staging_ns.allocation);
			m_index_staging_ns = {};
		}


		if (bytes == 0) {
			return nullptr;
		}

		// allocate memory
		m_index_staging_ns = ToolsVk::create_buffer(m_allocator, bytes, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VMA_MEMORY_USAGE_CPU_ONLY, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

		// map memory
		void* data;
		if (bytes) {
			VK_CHECK_RESULT(vmaMapMemory(m_allocator, m_index_staging_ns.allocation, &data));
		}
		return data;
	}
	void RendererVk::UnmapIndices()
	{
		spdlog::info("NS: UnmapIndices()");

		if (m_index_staging_ns.buffer == VK_NULL_HANDLE) return;

		vmaUnmapMemory(m_allocator, m_index_staging_ns.allocation);

		// Recreate a tight device-local IBO sized to the written bytes
		if (m_noesis.index_buffer.buffer != VK_NULL_HANDLE) {
			vmaDestroyBuffer(m_allocator, m_noesis.index_buffer.buffer, m_noesis.index_buffer.allocation);
			m_noesis.index_buffer = {};
		}

		vmaDestroyBuffer(m_allocator, m_index_staging_ns.buffer, m_index_staging_ns.allocation);
		m_index_staging_ns = {};
	}

	void RendererVk::DrawBatch(const Noesis::Batch& batch)
	{
		// If uploads didn't happen yet, skip safely
		if (m_noesis.vertex_buffer.buffer == VK_NULL_HANDLE || m_noesis.index_buffer.buffer == VK_NULL_HANDLE)
			return;

		VkCommandBuffer cmd = m_cmd_buffers[m_current_frame];

		// Bind VB/IB at offset 0 (we re-created them per Unmap*)
		const VkBuffer vbs[] = { m_noesis.vertex_buffer.buffer };
		const VkDeviceSize vboOffsets[] = { 0 };
		vkCmdBindVertexBuffers(cmd, 0, 1, vbs, vboOffsets);

		// Noesis indices are 16-bit
		vkCmdBindIndexBuffer(cmd, m_noesis.index_buffer.buffer, 0, VK_INDEX_TYPE_UINT16);

		const uint32_t firstIndex = batch.startIndex;   // base is 0 with this simple path
		const uint32_t indexCount = batch.numIndices;

		// NOTE: pipeline & descriptors are purposefully not (re)bound here.
		// Bind whatever pipeline/sets you want before calling Noesis to render.
		vkCmdDrawIndexed(cmd, indexCount, 1, firstIndex, 0, 0);
	}

