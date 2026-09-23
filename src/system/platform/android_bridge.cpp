/**
 * @file        system/platform/android_bridge.cpp
 * @brief       Direct NDK / JNI Bridge for Android application lifecycle & hardware I/O
 *
 * @copyright   Copyright (c) 2026 ReXGlue Authors
 * @license     BSD 3-Clause License
 */

#include <rex/platform/android/android_bridge.h>
#include <rex/logging.h>

namespace rex::platform::android {

static rex::ui::WindowAndroid* active_window_ = nullptr;

void AndroidBridge::SetActiveWindow(rex::ui::WindowAndroid* window) {
  active_window_ = window;
}

rex::ui::WindowAndroid* AndroidBridge::GetActiveWindow() {
  return active_window_;
}

void AndroidBridge::OnSurfaceCreated(ANativeWindow* native_window) {
  if (active_window_) {
    active_window_->SetNativeWindow(native_window);
  }
}

void AndroidBridge::OnSurfaceDestroyed() {
  if (active_window_) {
    active_window_->SetNativeWindow(nullptr);
  }
}

void AndroidBridge::OnSurfaceChanged(ANativeWindow* native_window, [[maybe_unused]] int width,
                                    [[maybe_unused]] int height) {
  if (active_window_) {
    active_window_->SetNativeWindow(native_window);
  }
}

void AndroidBridge::DispatchKeyEvent(int action, int key_code) {
  auto* driver = rex::input::android::AndroidInputDriver::GetInstance();
  if (driver) {
    driver->HandleKeyEvent(action, key_code);
  }
}

void AndroidBridge::DispatchMotionEvent(int axis, float value) {
  auto* driver = rex::input::android::AndroidInputDriver::GetInstance();
  if (driver) {
    driver->HandleMotionEvent(axis, value);
  }
}

void AndroidBridge::DispatchVirtualButton(uint16_t button_mask, bool pressed) {
  auto* driver = rex::input::android::AndroidInputDriver::GetInstance();
  if (driver) {
    driver->SetButtonState(button_mask, pressed);
  }
}

void AndroidBridge::DispatchVirtualStick(bool is_right, int16_t x, int16_t y) {
  auto* driver = rex::input::android::AndroidInputDriver::GetInstance();
  if (driver) {
    driver->SetThumb(is_right, x, y);
  }
}

void AndroidBridge::DispatchVirtualTrigger(bool is_right, uint8_t value) {
  auto* driver = rex::input::android::AndroidInputDriver::GetInstance();
  if (driver) {
    driver->SetTrigger(is_right, value);
  }
}

}  // namespace rex::platform::android

extern "C" {

JNIEXPORT void JNICALL Java_com_rexglue_runtime_NativeBridge_setSurface(
    JNIEnv* env, [[maybe_unused]] jclass clazz, jobject surface) {
  if (!surface) {
    rex::platform::android::AndroidBridge::OnSurfaceDestroyed();
    return;
  }
  ANativeWindow* window = ANativeWindow_fromSurface(env, surface);
  rex::platform::android::AndroidBridge::OnSurfaceCreated(window);
}

JNIEXPORT void JNICALL Java_com_rexglue_runtime_NativeBridge_destroySurface(
    [[maybe_unused]] JNIEnv* env, [[maybe_unused]] jclass clazz) {
  rex::platform::android::AndroidBridge::OnSurfaceDestroyed();
}

JNIEXPORT void JNICALL Java_com_rexglue_runtime_NativeBridge_dispatchKeyEvent(
    [[maybe_unused]] JNIEnv* env, [[maybe_unused]] jclass clazz, jint action, jint key_code) {
  rex::platform::android::AndroidBridge::DispatchKeyEvent(action, key_code);
}

JNIEXPORT void JNICALL Java_com_rexglue_runtime_NativeBridge_dispatchMotionEvent(
    [[maybe_unused]] JNIEnv* env, [[maybe_unused]] jclass clazz, jint axis, jfloat value) {
  rex::platform::android::AndroidBridge::DispatchMotionEvent(axis, value);
}

JNIEXPORT void JNICALL Java_com_rexglue_runtime_NativeBridge_setVirtualButton(
    [[maybe_unused]] JNIEnv* env, [[maybe_unused]] jclass clazz, jint button_mask, jboolean pressed) {
  rex::platform::android::AndroidBridge::DispatchVirtualButton(
      static_cast<uint16_t>(button_mask), pressed == JNI_TRUE);
}

JNIEXPORT void JNICALL Java_com_rexglue_runtime_NativeBridge_setVirtualStick(
    [[maybe_unused]] JNIEnv* env, [[maybe_unused]] jclass clazz, jboolean is_right, jshort x, jshort y) {
  rex::platform::android::AndroidBridge::DispatchVirtualStick(is_right == JNI_TRUE, x, y);
}

JNIEXPORT void JNICALL Java_com_rexglue_runtime_NativeBridge_setVirtualTrigger(
    [[maybe_unused]] JNIEnv* env, [[maybe_unused]] jclass clazz, jboolean is_right, jint value) {
  rex::platform::android::AndroidBridge::DispatchVirtualTrigger(
      is_right == JNI_TRUE, static_cast<uint8_t>(value));
}

}  // extern "C"
