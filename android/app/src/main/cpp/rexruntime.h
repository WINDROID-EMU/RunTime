#pragma once

#include <android/native_window.h>
#include <string>
#include <cstdint>
#include <atomic>
#include <thread>
#include <mutex>

namespace rex {

class AndroidRuntime {
public:
    static AndroidRuntime& Get();

    bool Initialize(const std::string& internal_path, const std::string& external_path);
    void Shutdown();

    void SetSurface(ANativeWindow* window);
    void DestroySurface();

    bool LoadModule(const std::string& module_path, const std::string& game_data, const std::string& save_data);
    void Pause();
    void Resume();
    bool IsRunning() const;

    float GetFps() const;
    std::string GetVersion() const;

    void DispatchKeyEvent(int action, int key_code);
    void DispatchMotionEvent(int axis, float value);
    void DispatchVirtualButton(uint16_t button_mask, bool pressed);
    void DispatchVirtualStick(bool is_right, int16_t x, int16_t y);
    void DispatchVirtualTrigger(bool is_right, uint8_t value);

private:
    AndroidRuntime();
    ~AndroidRuntime();

    void RenderLoop();

    std::atomic<bool> initialized_{false};
    std::atomic<bool> running_{false};
    std::atomic<bool> paused_{false};

    ANativeWindow* window_{nullptr};
    std::mutex surface_mutex_;

    std::thread render_thread_;
    std::atomic<float> current_fps_{60.0f};

    std::string internal_data_path_;
    std::string external_data_path_;
    std::string current_module_;
};

} // namespace rex
