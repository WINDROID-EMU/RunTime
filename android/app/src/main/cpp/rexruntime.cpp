#include "rexruntime.h"

#include <android/log.h>
#include <chrono>
#include <cstring>
#include <cmath>

#define LOG_TAG "ReXGlue.Native"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace rex {

AndroidRuntime& AndroidRuntime::Get() {
    static AndroidRuntime instance;
    return instance;
}

AndroidRuntime::AndroidRuntime() = default;

AndroidRuntime::~AndroidRuntime() {
    Shutdown();
}

bool AndroidRuntime::Initialize(const std::string& internal_path, const std::string& external_path) {
    if (initialized_.load()) {
        LOGI("AndroidRuntime already initialized");
        return true;
    }

    internal_data_path_ = internal_path;
    external_data_path_ = external_path;

    LOGI("AndroidRuntime initialized successfully");
    LOGI("  Internal: %s", internal_path.c_str());
    LOGI("  External: %s", external_path.c_str());

    initialized_.store(true);
    return true;
}

void AndroidRuntime::Shutdown() {
    if (!initialized_.load()) return;

    LOGI("AndroidRuntime shutting down...");
    running_.store(false);

    if (render_thread_.joinable()) {
        render_thread_.join();
    }

    DestroySurface();
    initialized_.store(false);
    LOGI("AndroidRuntime shutdown complete");
}

void AndroidRuntime::SetSurface(ANativeWindow* window) {
    std::lock_guard<std::mutex> lock(surface_mutex_);
    if (window_ == window) return;

    if (window_) {
        ANativeWindow_release(window_);
        window_ = nullptr;
    }

    if (window) {
        ANativeWindow_acquire(window);
        // Configure standard 32-bit RGBA presentation
        ANativeWindow_setBuffersGeometry(window, 0, 0, WINDOW_FORMAT_RGBA_8888);
        window_ = window;
        LOGI("Native surface attached: %p", window);
    }
}

void AndroidRuntime::DestroySurface() {
    std::lock_guard<std::mutex> lock(surface_mutex_);
    if (window_) {
        ANativeWindow_release(window_);
        window_ = nullptr;
        LOGI("Native surface released");
    }
}

bool AndroidRuntime::LoadModule(const std::string& module_path, const std::string& game_data, const std::string& save_data) {
    current_module_ = module_path;
    LOGI("Loading recompiled game module: %s", module_path.c_str());
    LOGI("  Game data: %s", game_data.c_str());
    LOGI("  Save data: %s", save_data.c_str());

    if (running_.load()) {
        running_.store(false);
        if (render_thread_.joinable()) {
            render_thread_.join();
        }
    }

    running_.store(true);
    paused_.store(false);

    render_thread_ = std::thread(&AndroidRuntime::RenderLoop, this);
    return true;
}

void AndroidRuntime::Pause() {
    LOGI("Game loop paused");
    paused_.store(true);
}

void AndroidRuntime::Resume() {
    LOGI("Game loop resumed");
    paused_.store(false);
}

bool AndroidRuntime::IsRunning() const {
    return running_.load();
}

float AndroidRuntime::GetFps() const {
    return current_fps_.load();
}

std::string AndroidRuntime::GetVersion() const {
    return "0.10.0-arm64";
}

void AndroidRuntime::DispatchKeyEvent(int action, int key_code) {
    LOGI("KeyEvent: action=%d, key=%d", action, key_code);
}

void AndroidRuntime::DispatchMotionEvent(int axis, float value) {
    // Only log occasionally to prevent spam
    if (std::abs(value) > 0.1f) {
        LOGI("MotionEvent: axis=%d, value=%.3f", axis, value);
    }
}

void AndroidRuntime::DispatchVirtualButton(uint16_t button_mask, bool pressed) {
    LOGI("VirtualButton: mask=0x%04X, pressed=%d", button_mask, pressed);
}

void AndroidRuntime::DispatchVirtualStick(bool is_right, int16_t x, int16_t y) {
    if (std::abs(x) > 2000 || std::abs(y) > 2000) {
        LOGI("VirtualStick: right=%d, x=%d, y=%d", is_right, x, y);
    }
}

void AndroidRuntime::DispatchVirtualTrigger(bool is_right, uint8_t value) {
    if (value > 10) {
        LOGI("VirtualTrigger: right=%d, value=%d", is_right, value);
    }
}

void AndroidRuntime::RenderLoop() {
    LOGI("Render thread started");

    int frame_count = 0;
    auto last_fps_time = std::chrono::steady_clock::now();
    uint32_t step = 0;

    while (running_.load()) {
        if (paused_.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            continue;
        }

        auto frame_start = std::chrono::steady_clock::now();

        // Lock native window and draw presentation frame
        {
            std::lock_guard<std::mutex> lock(surface_mutex_);
            if (window_) {
                ANativeWindow_Buffer buffer;
                if (ANativeWindow_lock(window_, &buffer, nullptr) == 0) {
                    if (buffer.bits && buffer.width > 0 && buffer.height > 0) {
                        uint32_t* pixels = static_cast<uint32_t*>(buffer.bits);
                        int stride = buffer.stride;
                        int width = buffer.width;
                        int height = buffer.height;

                        // Modern sleek render: dark background with subtle neon Xbox green pulse
                        float pulse = (std::sin(step * 0.05f) + 1.0f) * 0.5f; // 0.0 to 1.0
                        uint8_t green_val = static_cast<uint8_t>(20 + pulse * 40);
                        uint32_t bg_color = 0xFF000000 | (green_val << 8) | 0x0E;

                        for (int y = 0; y < height; ++y) {
                            uint32_t* row = pixels + y * stride;
                            for (int x = 0; x < width; ++x) {
                                row[x] = bg_color;
                            }
                        }
                    }
                    ANativeWindow_unlockAndPost(window_);
                }
            }
        }

        step++;
        frame_count++;

        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_fps_time).count();
        if (elapsed >= 1000) {
            float fps = static_cast<float>(frame_count) * 1000.0f / static_cast<float>(elapsed);
            current_fps_.store(fps);
            frame_count = 0;
            last_fps_time = now;
        }

        // Cap to ~60 FPS (16.6ms per frame)
        auto frame_end = std::chrono::steady_clock::now();
        auto frame_duration = std::chrono::duration_cast<std::chrono::milliseconds>(frame_end - frame_start).count();
        if (frame_duration < 16) {
            std::this_thread::sleep_for(std::chrono::milliseconds(16 - frame_duration));
        }
    }

    LOGI("Render thread terminated cleanly");
}

} // namespace rex
