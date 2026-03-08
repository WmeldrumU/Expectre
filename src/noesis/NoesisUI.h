#pragma once

#include "noesis/VKRenderDevice.h"
#include "noesis/VKFactory.h"
#include "observer.h"

#include <NsCore/Ptr.h>
#include <NsGui/IView.h>

#include <vulkan/vulkan.h>
#include <cstdint>
#include <cassert>
#include <memory>

namespace Expectre {

/// Singleton Noesis UI overlay.
///
/// Lifecycle:
///   1. NoesisUI::Init(info)   — call once after Vulkan is ready
///   2. NoesisUI::Instance()   — use from anywhere
///   3. NoesisUI::Shutdown()   — call before Vulkan teardown
///
/// Input: call HandleSDLEvent() from the main event loop.
/// Rendering: call PreRender() before the render pass, Render() inside it.
class NoesisUI {
public:
    struct InitInfo {
        VkInstance          instance;
        VkPhysicalDevice    physicalDevice;
        VkDevice            device;
        VkQueue             graphicsQueue;
        uint32_t            queueFamilyIndex;
        VkRenderPass        renderPass;       // the on-screen pass to draw into
        VkSampleCountFlagBits sampleCount = VK_SAMPLE_COUNT_1_BIT;
        uint32_t            width;
        uint32_t            height;
        uint32_t            maxFramesInFlight = 2;
    };

    explicit NoesisUI(const InitInfo& info);
    ~NoesisUI();

    NoesisUI(const NoesisUI&) = delete;
    NoesisUI& operator=(const NoesisUI&) = delete;

    /// Call once per frame BEFORE vkCmdBeginRenderPass.
    /// Handles SetCommandBuffer, Update, UpdateRenderTree, RenderOffscreen.
    void PreRender(VkCommandBuffer cmd, uint64_t frameNumber, double timeSeconds);

    /// Call once per frame INSIDE the render pass, after your scene draw.
    /// Handles SetRenderPass + IRenderer::Render.
    void Render();

    /// Resize the view (e.g. on swapchain recreate).
    void SetSize(uint32_t width, uint32_t height);

    /// Create an InputObserver that forwards SDL events to the Noesis view.
    /// The returned object's lifetime is managed by the caller (typically
    /// stored in InputManager's observer list).  No Noesis types leak out.
    std::shared_ptr<InputObserver> CreateInputAdapter(float dpiScale = 1.0f);

private:
    void queueSubmit(VkCommandBuffer cb);

    Noesis::Ptr<NoesisApp::VKRenderDevice> m_device;
    Noesis::Ptr<Noesis::IView>             m_view;

    VkQueue      m_graphicsQueue = VK_NULL_HANDLE;
    VkRenderPass m_renderPass    = VK_NULL_HANDLE;
    VkSampleCountFlagBits m_sampleCount = VK_SAMPLE_COUNT_1_BIT;
    uint32_t     m_maxFramesInFlight = 2;
};

} // namespace Expectre
