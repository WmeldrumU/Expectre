#include "NoesisUI.h"
#include "noesis/NoesisInputAdapter.h"

#include <NsCore/Init.h>
#include <NsGui/CachedFontProvider.h>
#include <NsGui/FrameworkElement.h>
#include <NsGui/IRenderer.h>
#include <NsGui/IntegrationAPI.h>
#include <NsGui/Stream.h>
#include <NsGui/Uri.h>
#include <NsGui/XamlProvider.h>

#include <cstdio>
#include <filesystem>

namespace fs = std::filesystem;

////////////////////////////////////////////////////////////////////////////////////////////////////
/// Minimal XAML provider that serves .xaml files from a directory on disk.
////////////////////////////////////////////////////////////////////////////////////////////////////
class LocalXamlProvider final : public Noesis::XamlProvider
{
public:
    LocalXamlProvider(const char* rootDir) : m_root(rootDir)
    {
        if (!m_root.empty() && m_root.back() != '/' && m_root.back() != '\\')
            m_root += '/';
    }

    Noesis::Ptr<Noesis::Stream> LoadXaml(const Noesis::Uri& uri) override
    {
        Noesis::String uriPath;
        uri.GetPath(uriPath);
        std::string path = m_root + uriPath.Str();
        printf("[NoesisUI] LoadXaml: %s\n", path.c_str());
        return Noesis::OpenFileStream(path.c_str());
    }

private:
    std::string m_root;
};

////////////////////////////////////////////////////////////////////////////////////////////////////
/// Minimal font provider that serves .ttf/.otf files from a directory on disk.
////////////////////////////////////////////////////////////////////////////////////////////////////
class LocalFontProvider final : public Noesis::CachedFontProvider
{
public:
    LocalFontProvider(const char* rootDir) : m_root(rootDir)
    {
        if (!m_root.empty() && m_root.back() != '/' && m_root.back() != '\\')
            m_root += '/';
    }

protected:
    void ScanFolder(const Noesis::Uri& folder) override
    {
        Noesis::String folderPath;
        folder.GetPath(folderPath);
        std::string dir = m_root + folderPath.Str();
        std::error_code ec;
        if (!fs::is_directory(dir, ec)) dir = m_root;
        for (const auto& e : fs::directory_iterator(dir, ec)) {
            if (!e.is_regular_file()) continue;
            auto ext = e.path().extension().string();
            for (auto& c : ext) c = (char)std::tolower(c);
            if (ext == ".ttf" || ext == ".otf" || ext == ".ttc")
                RegisterFont(folder, e.path().filename().string().c_str());
        }
    }

    Noesis::Ptr<Noesis::Stream> OpenFont(const Noesis::Uri& folder,
                                         const char* filename) const override
    {
        Noesis::String folderPath;
        folder.GetPath(folderPath);
        std::string dir = m_root + folderPath.Str();
        if (!dir.empty() && dir.back() != '/' && dir.back() != '\\')
            dir += '/';
        std::string path = dir + filename;
        printf("[NoesisUI] OpenFont: %s\n", path.c_str());
        return Noesis::OpenFileStream(path.c_str());
    }

private:
    std::string m_root;
};

namespace Expectre {

////////////////////////////////////////////////////////////////////////////////////////////////////
NoesisUI::NoesisUI(const InitInfo &info)
    : m_graphicsQueue(info.graphicsQueue), m_renderPass(info.renderPass),
      m_sampleCount(info.sampleCount),
      m_maxFramesInFlight(info.maxFramesInFlight) {
  // Note: VKRenderDevice constructor calls SetLicense + GUI::Init internally,
  // so we must NOT call them here (double-init is undefined behavior).

  // --- Create VKRenderDevice ---
  NoesisApp::VKFactory::InstanceInfo nsInfo{};
  nsInfo.instance = info.instance;
  nsInfo.physicalDevice = info.physicalDevice;
  nsInfo.device = info.device;
  nsInfo.queueFamilyIndex = info.queueFamilyIndex;
  nsInfo.vkGetInstanceProcAddr = ::vkGetInstanceProcAddr;
  nsInfo.QueueSubmit = Noesis::MakeDelegate(this, &NoesisUI::queueSubmit);

  m_device = *new NoesisApp::VKRenderDevice(true, nsInfo);

  // --- Register providers for our own assets (Page0.xaml, custom fonts) ---
  std::string noesisRoot = std::string(WORKSPACE_DIR) + "/assets/noesis";
  Noesis::GUI::SetXamlProvider(Noesis::MakePtr<LocalXamlProvider>(noesisRoot.c_str()));
  Noesis::GUI::SetFontProvider(Noesis::MakePtr<LocalFontProvider>(noesisRoot.c_str()));

  // --- Register providers for the Noesis theme assembly ---
  Noesis::GUI::SetAssemblyXamlProvider("Noesis.GUI.Extensions",
      Noesis::MakePtr<LocalXamlProvider>(noesisRoot.c_str()));
  Noesis::GUI::SetAssemblyFontProvider("Noesis.GUI.Extensions",
      Noesis::MakePtr<LocalFontProvider>(noesisRoot.c_str()));

  // --- Set font fallbacks (theme font + system fonts) ---
  const char* fallbacks[] = {
      "Theme/Fonts/#PT Root UI",  // Noesis theme font
      "./#Hermeneus One",         // Our custom font
      "Arial",                    // System fallback
      "Segoe UI Emoji",
  };
  Noesis::GUI::SetFontFallbacks(fallbacks, 4);
  Noesis::GUI::SetFontDefaultProperties(15.0f, Noesis::FontWeight_Normal,
      Noesis::FontStretch_Normal, Noesis::FontStyle_Normal);

  // --- Load application resources (includes theme + any custom app resources) ---
  Noesis::GUI::LoadApplicationResources(Noesis::Uri("GlobalResources.xaml"));

  // --- Load our own XAML via URI so providers get proper context ---
  auto root = Noesis::GUI::LoadXaml<Noesis::FrameworkElement>("Page0.xaml");

  m_view = Noesis::GUI::CreateView(root);
  m_view->SetSize(info.width, info.height);
  m_view->GetRenderer()->Init(m_device);

  // Pre-create pipelines compatible with the on-screen render pass
  m_device->WarmUpRenderPass(m_renderPass, m_sampleCount);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
NoesisUI::~NoesisUI() {
  if (m_view) {
    m_view->GetRenderer()->Shutdown();
    m_view.Reset();
  }
  m_device.Reset();
  Noesis::Shutdown();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void NoesisUI::PreRender(VkCommandBuffer cmd, uint64_t frameNumber,
                         double timeSeconds) {
  NoesisApp::VKFactory::RecordingInfo rec{};
  rec.commandBuffer = cmd;
  rec.frameNumber = frameNumber;
  rec.safeFrameNumber = frameNumber >= m_maxFramesInFlight
                            ? frameNumber - m_maxFramesInFlight
                            : 0;

  m_device->SetCommandBuffer(rec);

  m_view->Update(timeSeconds);
  m_view->GetRenderer()->UpdateRenderTree();
  m_view->GetRenderer()->RenderOffscreen();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void NoesisUI::Render() {
  m_device->SetRenderPass(m_renderPass, m_sampleCount);
  m_view->GetRenderer()->Render();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void NoesisUI::SetSize(uint32_t width, uint32_t height) {
  m_view->SetSize(width, height);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
std::shared_ptr<InputObserver> NoesisUI::CreateInputAdapter(float dpiScale) {
  return std::make_shared<NoesisInputAdapter>(m_view.GetPtr(), dpiScale);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void NoesisUI::queueSubmit(VkCommandBuffer cb) {
  VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO};
  si.commandBufferCount = 1;
  si.pCommandBuffers = &cb;
  vkQueueSubmit(m_graphicsQueue, 1, &si, VK_NULL_HANDLE);
  vkQueueWaitIdle(m_graphicsQueue);
}

} // namespace Expectre
