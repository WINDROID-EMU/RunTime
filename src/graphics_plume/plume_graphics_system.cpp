/**
 * @file        graphics_plume/plume_graphics_system.cpp
 * @brief       Plume Graphics System implementation
 */

#include "plume_graphics_system.h"
#include "plume_command_processor.h"

#include <rex/logging.h>
#include <rex/ui/graphics_provider.h>
#include <rex/ui/presenter.h>
#include <rex/ui/windowed_app_context.h>
#if defined(__ANDROID__)
#include <android/log.h>
#include <rex/ui/windowed_app_context_android.h>
#include <rex/ui/window_android.h>
#include <rex/platform/android/android_bridge.h>
#endif

#include <plume_render_interface.h>

namespace plume {
std::unique_ptr<RenderInterface> CreateVulkanInterface();
}

namespace rex::graphics_plume {

PlumeGraphicsSystem::PlumeGraphicsSystem() {
  REXLOG_INFO("PlumeGraphicsSystem: instantiated");
}

PlumeGraphicsSystem::~PlumeGraphicsSystem() {
  Shutdown();
}

X_STATUS PlumeGraphicsSystem::SetupPresentation(ui::WindowedAppContext* app_context) {
  REXLOG_INFO("PlumeGraphicsSystem::SetupPresentation");
  app_context_ = app_context;

  if (!plume_interface_) {
    plume_interface_ = ::plume::CreateVulkanInterface();
    if (!plume_interface_) {
      REXLOG_ERROR("PlumeGraphicsSystem: failed to create VulkanInterface");
      return X_STATUS_UNSUCCESSFUL;
    }
    REXLOG_INFO("PlumeGraphicsSystem: VulkanInterface created successfully");

    plume_device_ = plume_interface_->createDevice("");
    if (!plume_device_) {
      REXLOG_ERROR("PlumeGraphicsSystem: failed to create Vulkan RenderDevice");
      return X_STATUS_UNSUCCESSFUL;
    }
    REXLOG_INFO("PlumeGraphicsSystem: Vulkan RenderDevice created successfully (name: '{}')",
                plume_device_->getDescription().name);
  }

  return X_STATUS_SUCCESS;
}

::plume::RenderSwapChain* PlumeGraphicsSystem::plume_swapchain() {
  if (plume_swapchain_) {
    return plume_swapchain_.get();
  }

#if defined(__ANDROID__)
  ANativeWindow* native_win = rex::platform::android::AndroidBridge::GetNativeWindow();
  if (!native_win && app_context_) {
    auto* android_app = static_cast<ui::AndroidWindowedAppContext*>(app_context_);
    auto* win = android_app ? android_app->GetWindow() : nullptr;
    if (win) {
      native_win = win->GetNativeWindow();
    }
  }
  static uint32_t check_count = 0;
  if ((++check_count % 60) == 1) {
    __android_log_print(ANDROID_LOG_INFO, "PlumeDebug",
                        "plume_swapchain #%u: native_win=%p plume_dev=%p",
                        check_count, native_win, plume_device_.get());
  }
  if (native_win && plume_device_) {
    ::plume::RenderSwapChainDesc swap_desc(
        native_win,
        ::plume::RenderFormat::R8G8B8A8_UNORM,
        3
    );
    if (!plume_queue_) {
      plume_queue_ = plume_device_->createCommandQueue(::plume::RenderCommandListType::DIRECT);
    }
    if (plume_queue_) {
      plume_swapchain_ = plume_queue_->createSwapChain(swap_desc);
      bool resized = plume_swapchain_ ? plume_swapchain_->resize() : false;
      __android_log_print(ANDROID_LOG_INFO, "PlumeDebug",
                          "PlumeGraphicsSystem: SwapChain created and resized=%d for ANativeWindow (%dx%d)",
                          resized, ANativeWindow_getWidth(native_win), ANativeWindow_getHeight(native_win));
    } else {
      __android_log_print(ANDROID_LOG_ERROR, "PlumeDebug", "PlumeGraphicsSystem: Failed to create queue for swapchain");
    }
  }
#endif

  return plume_swapchain_.get();
}

void PlumeGraphicsSystem::CreateProvider(bool with_presentation) {
  REXLOG_INFO("PlumeGraphicsSystem::CreateProvider (with_presentation={})", with_presentation);
}

std::unique_ptr<rex::graphics::CommandProcessor> PlumeGraphicsSystem::CreateCommandProcessor() {
  REXLOG_INFO("PlumeGraphicsSystem::CreateCommandProcessor");
  return std::make_unique<PlumeCommandProcessor>(this, kernel_state_, plume_device_.get());
}

void PlumeGraphicsSystem::Present() {
  auto* sc = plume_swapchain();
  if (sc) {
    uint32_t texture_index = 0;
    if (sc->acquireTexture(nullptr, &texture_index)) {
      sc->present(texture_index, nullptr, 0);
    }
  }
}

void PlumeGraphicsSystem::Shutdown() {
  REXLOG_INFO("PlumeGraphicsSystem::Shutdown");
  plume_swapchain_.reset();
  plume_queue_.reset();
  plume_device_.reset();
  plume_interface_.reset();
  rex::graphics::GraphicsSystem::Shutdown();
}

}  // namespace rex::graphics_plume
