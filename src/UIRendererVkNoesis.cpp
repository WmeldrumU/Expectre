#include "UIRendererVkNoesis.h"
#include <NsRender/RenderDevice.h>
#include <NsCore/Noesis.h>
#include <NsRender/RenderTarget.h>

#include <vulkan/vulkan.h>

//#include "Shaders.h"

#include <NsCore/Log.h>
#include <NsCore/HighResTimer.h>
#include <NsCore/DynamicCast.h>
#include <NsCore/String.h>
//#include <NsApp/FastLZ.h>
#include <NsRender/Texture.h>
#include <NsRender/RenderTarget.h>

#include <stdio.h>
#include <inttypes.h>

// In Noesis, render targets are never fully used. They are always loaded with 'LOAD_OP_DONT_CARE'.
// When doing multisampling it is very important to only resolve affected regions. On tiled
// architectures, we can resolve the whole surface on writeback because only touched tiled will be
// really resolved. For desktop, we need to manually resolve each region with 'vkCmdResolveImage'.

#ifdef NS_PLATFORM_ANDROID
#define RESOLVE_ON_WRITEBACK 1
#else
#define RESOLVE_ON_WRITEBACK 0
#endif

#define DESCRIPTOR_POOL_MAX_SETS 128
#define PREALLOCATED_DYNAMIC_PAGES 2
#define CBUFFER_SIZE 16 * 1024

#define V(exp) \
    NS_MACRO_BEGIN \
        VkResult err_ = (exp); \
        NS_ASSERT(err_ == VK_SUCCESS); \
    NS_MACRO_END

#define LOAD_INSTANCE_FUNC(x) \
    NS_MACRO_BEGIN \
        NS_ASSERT(mInstance != nullptr); \
        x = (decltype(x))vkGetInstanceProcAddr(mInstance, #x); \
        NS_ASSERT(x != nullptr); \
    NS_MACRO_END

#define LOAD_INSTANCE_EXT_FUNC(x) \
    NS_MACRO_BEGIN \
        NS_ASSERT(mInstance != nullptr); \
        x = (decltype(x))vkGetInstanceProcAddr(mInstance, #x); \
    NS_MACRO_END

#define LOAD_DEVICE_FUNC(x) \
    NS_MACRO_BEGIN \
        NS_ASSERT(mDevice != nullptr); \
        x = (decltype(x))vkGetDeviceProcAddr(mDevice, #x); \
        NS_ASSERT(x != nullptr); \
    NS_MACRO_END

#define LOAD_DEVICE_EXT_FUNC(x) \
    NS_MACRO_BEGIN \
        NS_ASSERT(mDevice != nullptr); \
        x = (decltype(x))vkGetDeviceProcAddr(mDevice, #x); \
    NS_MACRO_END

#ifdef NS_PROFILE
#define VK_BEGIN_EVENT(...) \
        NS_MACRO_BEGIN \
            if (vkCmdDebugMarkerBeginEXT != nullptr) \
            { \
                char name[128]; \
                snprintf(name, sizeof(name), __VA_ARGS__); \
                VkDebugMarkerMarkerInfoEXT info{}; \
                info.sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_MARKER_INFO_EXT; \
                info.pMarkerName = name; \
                vkCmdDebugMarkerBeginEXT(mCommandBuffer, &info); \
            } \
            else if (vkCmdBeginDebugUtilsLabelEXT != nullptr) \
            { \
                char name[128]; \
                snprintf(name, sizeof(name), __VA_ARGS__); \
                VkDebugUtilsLabelEXT info{}; \
                info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT; \
                info.pLabelName = name; \
                vkCmdBeginDebugUtilsLabelEXT(mCommandBuffer, &info); \
            } \
        NS_MACRO_END
#define VK_END_EVENT() \
        NS_MACRO_BEGIN \
            if (vkCmdDebugMarkerEndEXT != nullptr) \
            { \
                vkCmdDebugMarkerEndEXT(mCommandBuffer); \
            } \
            else if (vkCmdEndDebugUtilsLabelEXT != nullptr) \
            { \
                vkCmdEndDebugUtilsLabelEXT(mCommandBuffer); \
            } \
        NS_MACRO_END
#define VK_NAME(obj, type, ...) \
        NS_MACRO_BEGIN \
            NS_ASSERT(mDevice != nullptr); \
            if (vkDebugMarkerSetObjectNameEXT != nullptr) \
            { \
                char name[128]; \
                snprintf(name, sizeof(name), __VA_ARGS__); \
                VkDebugMarkerObjectNameInfoEXT info{}; \
                info.sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_OBJECT_NAME_INFO_EXT; \
                info.objectType = VK_DEBUG_REPORT_OBJECT_TYPE_##type##_EXT; \
                info.object = (uint64_t)obj; \
                info.pObjectName = name; \
                V(vkDebugMarkerSetObjectNameEXT(mDevice, &info)); \
            } \
            else if (vkSetDebugUtilsObjectNameEXT != nullptr) \
            { \
                char name[128]; \
                snprintf(name, sizeof(name), __VA_ARGS__); \
                VkDebugUtilsObjectNameInfoEXT info{}; \
                info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT; \
                info.objectType = VK_OBJECT_TYPE_##type; \
                info.objectHandle = (uint64_t)obj; \
                info.pObjectName = name; \
                V(vkSetDebugUtilsObjectNameEXT(mDevice, &info)); \
            } \
        NS_MACRO_END
#else
#define VK_BEGIN_EVENT(...) NS_NOOP
#define VK_END_EVENT() NS_NOOP
#define VK_NAME(obj, type, ...) NS_UNUSED(__VA_ARGS__)
#endif

namespace Expectre {


	////////////////////////////////////////////////////////////////////////////////////////////////////
	class VKTexture final : public Noesis::Texture
	{
	public:
		VKTexture(uint32_t hash_) : hash(hash_) {}

		~VKTexture()
		{
			if (device)
			{
				device->SafeReleaseTexture(this);
			}
		}

		uint32_t GetWidth() const override { return width; }
		uint32_t GetHeight() const override { return height; }
		bool HasMipMaps() const override { return levels > 1; }
		bool IsInverted() const override { return isInverted; }
		bool HasAlpha() const override { return hasAlpha; }

		VkImage image = VK_NULL_HANDLE;
		VkImageView view = VK_NULL_HANDLE;
		VkDeviceMemory memory = VK_NULL_HANDLE;

		uint32_t width = 0;
		uint32_t height = 0;
		uint32_t levels = 0;

		UIRendererVkNoesis* device = VK_NULL_HANDLE;

		VkFormat format = VK_FORMAT_UNDEFINED;
		VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;

		bool hasAlpha = true;
		bool isInverted = false;

		uint32_t hash = 0;
	};

	////////////////////////////////////////////////////////////////////////////////////////////////////
	class VKRenderTarget final : public Noesis::RenderTarget
	{
	public:
		~VKRenderTarget()
		{
			color->device->SafeReleaseRenderTarget(this);
		}

		Noesis::Texture* GetTexture() override { return color; }

		Noesis::Ptr<VKTexture> color;
		Noesis::Ptr<VKTexture> colorAA;
		Noesis::Ptr<VKTexture> stencil;

		VkFramebuffer framebuffer = VK_NULL_HANDLE;
		VkRenderPass renderPass = VK_NULL_HANDLE;
		VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT;
	};

	////////////////////////////////////////////////////////////////////////////////////////////////////
	const Noesis::DeviceCaps& UIRendererVkNoesis::GetCaps() const
	{
		return m_caps;
	}

	////////////////////////////////////////////////////////////////////////////////////////////////////
	static VkSampleCountFlagBits GetSampleCount(uint32_t samples, const VkPhysicalDeviceLimits& limits)
	{
		auto sampleCounts = limits.framebufferColorSampleCounts & limits.framebufferStencilSampleCounts;

		for (uint32_t bits = VK_SAMPLE_COUNT_64_BIT; bits > VK_SAMPLE_COUNT_1_BIT; bits >>= 1)
		{
			if (samples >= bits && (sampleCounts & bits) > 0)
			{
				return (VkSampleCountFlagBits)bits;
			}
		}

		return VK_SAMPLE_COUNT_1_BIT;
	}

	////////////////////////////////////////////////////////////////////////////////////////////////////
	static uint32_t Index(VkSampleCountFlagBits samples)
	{
		switch (samples)
		{
		case VK_SAMPLE_COUNT_1_BIT:  return 0;
		case VK_SAMPLE_COUNT_2_BIT:  return 1;
		case VK_SAMPLE_COUNT_4_BIT:  return 2;
		case VK_SAMPLE_COUNT_8_BIT:  return 3;
		case VK_SAMPLE_COUNT_16_BIT: return 4;
		case VK_SAMPLE_COUNT_32_BIT: return 5;
		case VK_SAMPLE_COUNT_64_BIT: return 6;
		default:
			NS_ASSERT_UNREACHABLE;
		}
	}
	//
	//	////////////////////////////////////////////////////////////////////////////////////////////////////
		Noesis::Ptr<Noesis::RenderTarget> UIRendererVkNoesis::CreateRenderTarget(const char* label, uint32_t width,
			uint32_t height, uint32_t samples_, bool needsStencil)
		{
			Noesis::Ptr<VKRenderTarget> surface = Noesis::MakePtr<VKRenderTarget>();
			surface->samples = GetSampleCount(samples_, mDeviceProperties.limits);
	
			EnsureTransferCommands();
	
			Noesis::Vector<VkImageView, 3> attachments;
	
			if (needsStencil)
			{
				surface->stencil = StaticPtrCast<VKTexture>(CreateTexture(label, width, height, 1,
					mStencilFormat, surface->samples, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
					VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT |
					VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT, VK_IMAGE_ASPECT_STENCIL_BIT));
				attachments.PushBack(surface->stencil->view);
				ChangeLayout(mTransferCommands, surface->stencil, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
			}
	
			if (surface->samples > 1)
			{
	#if RESOLVE_ON_WRITEBACK
				// Multisample surface
				surface->colorAA = StaticPtrCast<VKTexture>(CreateTexture(label, width, height, 1,
					mBackBufferFormat, surface->samples, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
					VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT |
					VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT, VK_IMAGE_ASPECT_COLOR_BIT));
				ChangeLayout(mTransferCommands, surface->colorAA, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
				attachments.PushBack(surface->colorAA->view);
	
				// Resolve surface
				surface->color = StaticPtrCast<VKTexture>(CreateTexture(label, width, height, 1,
					mBackBufferFormat, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_USAGE_SAMPLED_BIT,
					VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_IMAGE_ASPECT_COLOR_BIT));
				ChangeLayout(mTransferCommands, surface->color, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
				attachments.PushBack(surface->color->view);
	#else
				// Multisample surface
				surface->colorAA = StaticPtrCast<VKTexture>(CreateTexture(label, width, height, 1,
					mBackBufferFormat, surface->samples, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
					VK_IMAGE_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
					VK_IMAGE_ASPECT_COLOR_BIT));
				ChangeLayout(mTransferCommands, surface->colorAA, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
	
				// Resolve surface
				surface->color = StaticPtrCast<VKTexture>(CreateTexture(label, width, height, 1,
					mBackBufferFormat, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_USAGE_SAMPLED_BIT |
					VK_IMAGE_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
					VK_IMAGE_ASPECT_COLOR_BIT));
				attachments.PushBack(surface->colorAA->view);
	#endif
			}
			else
			{
				// XamlTester needs VK_IMAGE_USAGE_TRANSFER_SRC_BIT for grabbing screenshots
				surface->color = StaticPtrCast<VKTexture>(CreateTexture(label, width, height, 1,
					mBackBufferFormat, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
					VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
					VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_IMAGE_ASPECT_COLOR_BIT));
				attachments.PushBack(surface->color->view);
				ChangeLayout(mTransferCommands, surface->color, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
			}
	
			uint32_t index = Index(surface->samples);
			uint32_t stencilCount = needsStencil ? 1 : 0;
			NS_ASSERT(index < NS_COUNTOF(mRenderPasses));
	
			if (NS_UNLIKELY(mRenderPasses[index][stencilCount] == VK_NULL_HANDLE))
			{
				VkRenderPass renderPass = CreateRenderPass(surface->samples, needsStencil);
				CreatePipelines(renderPass, surface->samples);
				mRenderPasses[index][stencilCount] = renderPass;
			}
	
			surface->renderPass = mRenderPasses[index][stencilCount];
	
			VkFramebufferCreateInfo framebufferInfo{};
			framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			framebufferInfo.renderPass = surface->renderPass;
			framebufferInfo.attachmentCount = attachments.Size();
			framebufferInfo.pAttachments = attachments.Data();
			framebufferInfo.width = width;
			framebufferInfo.height = height;
			framebufferInfo.layers = 1;
	
			V(vkCreateFramebuffer(mDevice, &framebufferInfo, nullptr, &surface->framebuffer));
			VK_NAME(surface->framebuffer, FRAMEBUFFER, "Noesis_%s_FrameBuffer", label);
			return surface;
		}
	//
	//	////////////////////////////////////////////////////////////////////////////////////////////////////
	//	Ptr<RenderTarget> UIRendererVkNoesis::CloneRenderTarget(const char* label, RenderTarget* surface_)
	//	{
	//		VKRenderTarget* src = (VKRenderTarget*)surface_;
	//
	//		uint32_t width = src->color->width;
	//		uint32_t height = src->color->height;
	//
	//		Ptr<VKRenderTarget> surface = MakePtr<VKRenderTarget>();
	//		surface->renderPass = src->renderPass;
	//		surface->samples = src->samples;
	//		surface->colorAA = src->colorAA;
	//		surface->stencil = src->stencil;
	//
	//		Vector<VkImageView, 3> attachments;
	//		if (surface->stencil) { attachments.PushBack(surface->stencil->view); }
	//
	//		EnsureTransferCommands();
	//
	//		if (src->samples > 1)
	//		{
	//#if RESOLVE_ON_WRITEBACK
	//			surface->color = StaticPtrCast<VKTexture>(CreateTexture(label, width, height, 1,
	//				src->color->format, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_USAGE_SAMPLED_BIT,
	//				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_IMAGE_ASPECT_COLOR_BIT));
	//			ChangeLayout(mTransferCommands, surface->color, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	//			attachments.PushBack(surface->colorAA->view);
	//			attachments.PushBack(surface->color->view);
	//
	//#else
	//			surface->color = StaticPtrCast<VKTexture>(CreateTexture(label, width, height, 1,
	//				src->color->format, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_USAGE_SAMPLED_BIT |
	//				VK_IMAGE_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
	//				VK_IMAGE_ASPECT_COLOR_BIT));
	//			ChangeLayout(mTransferCommands, surface->color, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	//			attachments.PushBack(surface->colorAA->view);
	//#endif
	//		}
	//		else
	//		{
	//			surface->color = StaticPtrCast<VKTexture>(CreateTexture(label, width, height, 1,
	//				src->color->format, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
	//				VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
	//				VK_IMAGE_ASPECT_COLOR_BIT));
	//			ChangeLayout(mTransferCommands, surface->color, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	//			attachments.PushBack(surface->color->view);
	//		}
	//
	//		VkFramebufferCreateInfo framebufferInfo{};
	//		framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	//		framebufferInfo.renderPass = surface->renderPass;
	//		framebufferInfo.attachmentCount = attachments.Size();
	//		framebufferInfo.pAttachments = attachments.Data();
	//		framebufferInfo.width = width;
	//		framebufferInfo.height = height;
	//		framebufferInfo.layers = 1;
	//
	//		V(vkCreateFramebuffer(mDevice, &framebufferInfo, nullptr, &surface->framebuffer));
	//		VK_NAME(surface->framebuffer, FRAMEBUFFER, "Noesis_%s_FrameBuffer", label);
	//		return surface;
	//	}
	//
	//	////////////////////////////////////////////////////////////////////////////////////////////////////
	//	static VkFormat VKFormat(TextureFormat::Enum format, bool sRGB)
	//	{
	//		switch (format)
	//		{
	//		case TextureFormat::RGBA8: return sRGB ? VK_FORMAT_R8G8B8A8_SRGB : VK_FORMAT_R8G8B8A8_UNORM;
	//		case TextureFormat::RGBX8: return sRGB ? VK_FORMAT_R8G8B8A8_SRGB : VK_FORMAT_R8G8B8A8_UNORM;
	//		case TextureFormat::R8: return VK_FORMAT_R8_UNORM;
	//		default: NS_ASSERT_UNREACHABLE;
	//		}
	//	}
	//
	//	////////////////////////////////////////////////////////////////////////////////////////////////////
	//	static uint32_t Align(uint32_t n, uint32_t alignment)
	//	{
	//		NS_ASSERT(IsPow2(alignment));
	//		return (n + (alignment - 1)) & ~(alignment - 1);
	//	}
	//
	//	////////////////////////////////////////////////////////////////////////////////////////////////////
	//	Ptr<Texture> UIRendererVkNoesis::CreateTexture(const char* label, uint32_t width, uint32_t height,
	//		uint32_t numLevels, TextureFormat::Enum format_, const void** data)
	//	{
	//		VkFormat format = VKFormat(format_, m_caps.linearRendering);
	//		Ptr<VKTexture> texture = StaticPtrCast<VKTexture>(CreateTexture(label, width, height, numLevels,
	//			format, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
	//			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_IMAGE_ASPECT_COLOR_BIT));
	//		texture->hasAlpha = format_ == TextureFormat::RGBA8;
	//
	//		if (data != nullptr)
	//		{
	//			uint32_t texelBytes = texture->format == VK_FORMAT_R8_UNORM ? 1 : 4;
	//			uint32_t bufferSize = 0;
	//
	//			for (uint32_t i = 0; i < numLevels; i++)
	//			{
	//				uint32_t w = Max(width >> i, 1U);
	//				uint32_t h = Max(height >> i, 1U);
	//
	//				bufferSize += Align(w * h * texelBytes, 4);
	//			}
	//
	//			VkBufferCreateInfo bufferInfo{};
	//			bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	//			bufferInfo.size = bufferSize;
	//			bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	//			bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	//
	//			VkBuffer buffer;
	//			V(vkCreateBuffer(mDevice, &bufferInfo, nullptr, &buffer));
	//
	//			VkMemoryRequirements memRequirements;
	//			vkGetBufferMemoryRequirements(mDevice, buffer, &memRequirements);
	//
	//			VkMemoryAllocateInfo allocInfo{};
	//			allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	//			allocInfo.allocationSize = memRequirements.size;
	//			allocInfo.memoryTypeIndex = FindMemoryType(memRequirements.memoryTypeBits,
	//				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	//
	//			VkDeviceMemory memory;
	//			V(vkAllocateMemory(mDevice, &allocInfo, nullptr, &memory));
	//			V(vkBindBufferMemory(mDevice, buffer, memory, 0));
	//
	//			void* ptr;
	//			V(vkMapMemory(mDevice, memory, 0, VK_WHOLE_SIZE, 0, &ptr));
	//
	//			uint32_t offset = 0;
	//			Vector<VkBufferImageCopy, 32> regions;
	//
	//			for (uint32_t i = 0; i < numLevels; i++)
	//			{
	//				uint32_t w = Max(width >> i, 1U);
	//				uint32_t h = Max(height >> i, 1U);
	//				uint32_t size = w * h * texelBytes;
	//
	//				VkBufferImageCopy region{};
	//				region.imageExtent.width = w;
	//				region.imageExtent.height = h;
	//				region.imageExtent.depth = 1;
	//				region.imageSubresource.mipLevel = i;
	//				region.bufferOffset = offset;
	//				region.imageSubresource.layerCount = 1;
	//				region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	//
	//				regions.PushBack(region);
	//
	//				memcpy((uint8_t*)ptr + offset, data[i], size);
	//				offset += Align(size, 4);
	//			}
	//
	//			EnsureTransferCommands();
	//			ChangeLayout(mTransferCommands, texture, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
	//
	//			vkCmdCopyBufferToImage(mTransferCommands, buffer, texture->image,
	//				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, regions.Size(), regions.Data());
	//
	//			SafeReleaseBuffer(buffer, memory);
	//			mUpdatedTextures.PushBack(texture);
	//		}
	//
	//		return texture;
	//	}
	//
	//	////////////////////////////////////////////////////////////////////////////////////////////////////
	//	void UIRendererVkNoesis::UpdateTexture(Texture* texture_, uint32_t level, uint32_t x, uint32_t y,
	//		uint32_t width, uint32_t height, const void* data)
	//	{
	//		VKTexture* texture = (VKTexture*)texture_;
	//
	//		uint32_t texelBytes = texture->format == VK_FORMAT_R8_UNORM ? 1 : 4;
	//		uint32_t size = width * height * texelBytes;
	//		void* memory = MapBuffer(mTexUpload, Align(size, 4));
	//		memcpy(memory, data, size);
	//
	//		EnsureTransferCommands();
	//		ChangeLayout(mTransferCommands, texture, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
	//
	//		VkBufferImageCopy region{};
	//		region.imageOffset.x = x;
	//		region.imageOffset.y = y;
	//		region.imageExtent.width = width;
	//		region.imageExtent.height = height;
	//		region.imageExtent.depth = 1;
	//		region.imageSubresource.mipLevel = level;
	//		region.bufferOffset = mTexUpload.drawPos;
	//		region.imageSubresource.layerCount = 1;
	//		region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	//
	//		vkCmdCopyBufferToImage(mTransferCommands, mTexUpload.currentPage->buffer, texture->image,
	//			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
	//
	//		if (mUpdatedTextures.Find(texture) == mUpdatedTextures.End())
	//		{
	//			mUpdatedTextures.PushBack(texture);
	//		}
	//	}
	//
	//	////////////////////////////////////////////////////////////////////////////////////////////////////
	//	void UIRendererVkNoesis::BeginOffscreenRender()
	//	{
	//		VK_BEGIN_EVENT("Noesis.Offscreen");
	//	}
	//
	//	////////////////////////////////////////////////////////////////////////////////////////////////////
	//	void UIRendererVkNoesis::EndOffscreenRender()
	//	{
	//		FlushTransferCommands();
	//		VK_END_EVENT();
	//	}
	//
	//	////////////////////////////////////////////////////////////////////////////////////////////////////
	//	void UIRendererVkNoesis::BeginOnscreenRender()
	//	{
	//		ProcessPendingDestroys(false);
	//		VK_BEGIN_EVENT("Noesis");
	//	}
	//
	//	////////////////////////////////////////////////////////////////////////////////////////////////////
	//	void UIRendererVkNoesis::EndOnscreenRender()
	//	{
	//		FlushTransferCommands();
	//		VK_END_EVENT();
	//	}
	//
	//	////////////////////////////////////////////////////////////////////////////////////////////////////
	//	void UIRendererVkNoesis::SetRenderTarget(RenderTarget* surface_)
	//	{
	//		VKRenderTarget* surface = (VKRenderTarget*)surface_;
	//		VK_BEGIN_EVENT("SetRenderTarget");
	//
	//		VkViewport viewport{};
	//		viewport.x = 0.0f;
	//		viewport.y = 0.0f;
	//		viewport.width = (float)surface->color->width;
	//		viewport.height = (float)surface->color->height;
	//		viewport.minDepth = 0.0f;
	//		viewport.maxDepth = 1.0f;
	//
	//		vkCmdSetViewport(mCommandBuffer, 0, 1, &viewport);
	//	}
	//
	//	////////////////////////////////////////////////////////////////////////////////////////////////////
	//	void UIRendererVkNoesis::BeginTile(RenderTarget* surface_, const Tile& tile)
	//	{
	//		VKRenderTarget* surface = (VKRenderTarget*)surface_;
	//
	//		VkRect2D renderArea;
	//		renderArea.offset.x = tile.x;
	//		renderArea.offset.y = surface->color->height - (tile.y + tile.height);
	//		renderArea.extent.width = tile.width;
	//		renderArea.extent.height = tile.height;
	//
	//		VkRenderPassBeginInfo renderPassInfo{};
	//		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	//		renderPassInfo.renderPass = surface->renderPass;
	//		renderPassInfo.framebuffer = surface->framebuffer;
	//		renderPassInfo.renderArea = renderArea;
	//
	//		vkCmdBeginRenderPass(mCommandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
	//		vkCmdSetScissor(mCommandBuffer, 0, 1, &renderArea);
	//
	//		mActiveRenderPass = surface->renderPass;
	//	}
	//
	//	////////////////////////////////////////////////////////////////////////////////////////////////////
	//	void UIRendererVkNoesis::EndTile(RenderTarget*)
	//	{
	//		mActiveRenderPass = VK_NULL_HANDLE;
	//		vkCmdEndRenderPass(mCommandBuffer);
	//	}
	//
	//	////////////////////////////////////////////////////////////////////////////////////////////////////
	//	void UIRendererVkNoesis::ResolveRenderTarget(RenderTarget* surface_, const Tile* tiles, uint32_t numTiles)
	//	{
	//		VK_END_EVENT();
	//
	//#if !RESOLVE_ON_WRITEBACK
	//		VKRenderTarget* surface = (VKRenderTarget*)surface_;
	//
	//		if (surface->samples > 1)
	//		{
	//			Vector<VkImageResolve, 32> regions;
	//
	//			VKTexture* src = surface->colorAA;
	//			VKTexture* dst = surface->color;
	//
	//			for (uint32_t i = 0; i < numTiles; i++)
	//			{
	//				VkImageResolve& region = regions.EmplaceBack();
	//
	//				region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	//				region.srcSubresource.mipLevel = 0;
	//				region.srcSubresource.baseArrayLayer = 0;
	//				region.srcSubresource.layerCount = 1;
	//
	//				region.srcOffset.x = tiles[i].x;
	//				region.srcOffset.y = dst->height - tiles[i].y - tiles[i].height;
	//				region.srcOffset.z = 0;;
	//
	//				region.dstSubresource = region.srcSubresource;
	//				region.dstOffset = region.srcOffset;
	//
	//				region.extent.width = tiles[i].width;
	//				region.extent.height = tiles[i].height;
	//				region.extent.depth = 1;
	//			}
	//
	//			ChangeLayout(mCommandBuffer, dst, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
	//			vkCmdResolveImage(mCommandBuffer, src->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
	//				dst->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, regions.Size(), regions.Data());
	//			ChangeLayout(mCommandBuffer, dst, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	//		}
	//#else
	//		NS_UNUSED(surface_, tiles, numTiles);
	//#endif
	//	}
	//
	//	////////////////////////////////////////////////////////////////////////////////////////////////////
	//	void* UIRendererVkNoesis::MapVertices(uint32_t bytes)
	//	{
	//		return MapBuffer(mVertices, bytes);
	//	}
	//
	//	////////////////////////////////////////////////////////////////////////////////////////////////////
	//	void UIRendererVkNoesis::UnmapVertices()
	//	{
	//	}
	//
	//	////////////////////////////////////////////////////////////////////////////////////////////////////
	//	void* UIRendererVkNoesis::MapIndices(uint32_t bytes)
	//	{
	//		return MapBuffer(mIndices, bytes);
	//	}
	//
	//	////////////////////////////////////////////////////////////////////////////////////////////////////
	//	void UIRendererVkNoesis::UnmapIndices()
	//	{
	//	}
	//
	//	////////////////////////////////////////////////////////////////////////////////////////////////////
	//	static uint32_t GetSignature(const Batch& batch)
	//	{
	//		uint32_t signature = 0;
	//
	//		if (batch.pattern) signature |= PS_T0;
	//		if (batch.ramps) signature |= PS_T1;
	//		if (batch.image) signature |= PS_T2;
	//		if (batch.glyphs) signature |= PS_T3;
	//		if (batch.shadow) signature |= PS_T4;
	//		if (batch.vertexUniforms[0].values) signature |= VS_CB0;
	//		if (batch.vertexUniforms[1].values) signature |= VS_CB1;
	//		if (batch.pixelUniforms[0].values) signature |= PS_CB0;
	//		if (batch.pixelUniforms[1].values) signature |= PS_CB1;
	//
	//		return signature;
	//	}
	//
	//	////////////////////////////////////////////////////////////////////////////////////////////////////
	//	void UIRendererVkNoesis::DrawBatch(const Batch& batch)
	//	{
	//		NS_ASSERT(!batch.singlePassStereo || mStereoSupport);
	//
	//		Layout layout;
	//
	//		if (batch.pixelShader == 0)
	//		{
	//			NS_ASSERT(batch.shader.v < NS_COUNTOF(mLayouts));
	//			layout = mLayouts[batch.shader.v];
	//		}
	//		else
	//		{
	//			NS_ASSERT((uintptr_t)batch.pixelShader <= mCustomShaders.Size());
	//			layout = mCustomShaders[(int)(uintptr_t)batch.pixelShader - 1].layout;
	//		}
	//
	//		// Skip draw if shader requires something not available in the batch
	//		if (NS_UNLIKELY((layout.signature & GetSignature(batch)) != layout.signature)) return;
	//
	//		SetBuffers(batch);
	//		BindDescriptors(batch, layout);
	//		BindPipeline(batch);
	//		SetStencilRef(batch.stencilRef);
	//
	//		uint32_t firstIndex = batch.startIndex + mIndices.drawPos / 2;
	//
	//		if (batch.singlePassStereo)
	//		{
	//#ifdef NS_PLATFORM_ANDROID
	//			// GL_EXT_multiview
	//			vkCmdDrawIndexed(mCommandBuffer, batch.numIndices, 1, firstIndex, 0, 0);
	//#else
	//			// GL_ARB_shader_viewport_layer_array
	//			vkCmdDrawIndexed(mCommandBuffer, batch.numIndices, 2, firstIndex, 0, 0);
	//#endif
	//		}
	//		else
	//		{
	//			vkCmdDrawIndexed(mCommandBuffer, batch.numIndices, 1, firstIndex, 0, 0);
	//		}
	//	}
}