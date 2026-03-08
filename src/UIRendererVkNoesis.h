#ifndef UI_RENDERER_VK_NOESIS_H
#define UI_RENDERER_VK_NOESIS_H
#include <NsRender/RenderDevice.h>
//#include <NsRender/VKFactory.h>
#include <NsCore/HashMap.h>
#include <NsCore/Vector.h>
#include <NsCore/String.h>
#include <vulkan/vulkan.h>

namespace Expectre {
	class UIRendererVkNoesis : public Noesis::RenderDevice {
		/// From RenderDevice
//@{
	public:
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
	private:
		Noesis::DeviceCaps m_caps;


		bool mStereoSupport;
		bool mHasExtendedDynamicState;
		VkPhysicalDeviceMemoryProperties mMemoryProperties;
		VkPhysicalDeviceFeatures mDeviceFeatures;
		VkPhysicalDeviceProperties mDeviceProperties;
		VkFormat mStencilFormat;
		VkFormat mBackBufferFormat;

		struct Page
		{
			uint32_t hash;
			VkBuffer buffer;
			VkDeviceMemory memory;
			void* base;
			uint64_t frameNumber;
			Page* next;
		};

		struct PageAllocator
		{
			struct Block
			{
				Page pages[16];
				uint32_t count;
				Block* next;
			};

			Block* blocks = nullptr;
		};

		struct DynamicBuffer
		{
			uint32_t size;
			uint32_t pos;
			uint32_t drawPos;

			const char* label;

			Page* currentPage;
			Page* freePages;
			Page* pendingPages;

			PageAllocator allocator;

			uint32_t numPages;
			VkBufferUsageFlags usage;
			VkMemoryPropertyFlags memFlags;
		};

		DynamicBuffer mVertices;
		DynamicBuffer mIndices;
		DynamicBuffer mConstants[4];
		DynamicBuffer mTexUpload;

		struct Layout
		{
			uint32_t signature;
			VkDescriptorSetLayout setLayout;
			VkPipelineLayout pipelineLayout;
		};

		Noesis::HashMap<uint32_t, Layout, 16> mLayoutMap;
		Noesis::HashMap<VkRenderPass, VkSampleCountFlagBits> mCachedPipelineRenderPasses;
		Noesis::HashMap<uint32_t, uint32_t> mPipelineMap;
		Noesis::Vector<VkPipeline> mPipelines;

		VkShaderModule mVertexShaders[Noesis::Shader::Vertex::Count];
		VkShaderModule mPixelShaders[Noesis::Shader::Count];
		Layout mLayouts[Noesis::Shader::Count];

		struct CustomShader
		{
			Noesis::String label;
			uint8_t id;
			Layout layout;
			VkShaderModule module;
		};

		Noesis::Vector<CustomShader> mCustomShaders;

		VkDescriptorPool mDescriptorPool = VK_NULL_HANDLE;
		Noesis::Vector<Noesis::Pair<VkDescriptorPool, uint64_t>> mFreeDescriptorPools;
		Noesis::HashMap<uint32_t, VkDescriptorSet> mDescriptorSetMap;

		VkSampler mSamplers[64];
		VkRenderPass mRenderPasses[6][2] = {};

		uint64_t mFrameNumber;
		uint64_t mSafeFrameNumber;

		struct PendingDestroy
		{
			uint64_t frame;

			VkImage image;
			VkBuffer buffer;
			VkImageView view;
			VkDeviceMemory memory;

			VkFramebuffer framebuffer;
		};

		Noesis::Vector<PendingDestroy> mPendingDestroys;
		uint32_t mLastTextureHashValue = 1;
		uint32_t mLastBufferHashValue = 1;

		uint32_t mCachedStencilRef;
		uint32_t mCachedDepthTestEnable;
		uint32_t mCachedStencilTestEnable;
		uint32_t mCachedStencilOp;
		uint32_t mCachedConstantHash[4];
		VkBuffer mCachedIndexBuffer;
		VkPipeline mCachedPipeline;
	};
}

#endif UI_RENDERER_VK_NOESIS_H