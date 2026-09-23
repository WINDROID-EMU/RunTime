/**
 * @file        audio/android/aaudio_system.cpp
 * @brief       AAudio implementation of the AudioSystem abstraction for Android
 *
 * @copyright   Copyright (c) 2026 ReXGlue Authors
 * @license     BSD 3-Clause License
 */

#include <rex/audio/android/aaudio_system.h>
#include <rex/audio/android/aaudio_driver.h>
#include <rex/logging.h>

namespace rex::audio::android {

std::unique_ptr<AudioSystem> AAudioSystem::Create(runtime::FunctionDispatcher* function_dispatcher) {
  return std::make_unique<AAudioSystem>(function_dispatcher);
}

AAudioSystem::AAudioSystem(runtime::FunctionDispatcher* function_dispatcher)
    : AudioSystem(function_dispatcher) {}

AAudioSystem::~AAudioSystem() = default;

void AAudioSystem::Initialize() {
  AudioSystem::Initialize();
}

X_STATUS AAudioSystem::CreateDriver([[maybe_unused]] size_t index,
                                    rex::thread::Semaphore* semaphore,
                                    AudioDriver** out_driver) {
  assert_not_null(out_driver);
  auto driver = std::make_unique<AAudioDriver>(memory_, semaphore);
  if (!driver->Initialize()) {
    REXAPU_ERROR("AAudioDriver::Initialize failed");
    return X_STATUS_UNSUCCESSFUL;
  }
  *out_driver = driver.release();
  return X_STATUS_SUCCESS;
}

void AAudioSystem::DestroyDriver(AudioDriver* driver) {
  assert_not_null(driver);
  auto aaudio_driver = static_cast<AAudioDriver*>(driver);
  aaudio_driver->Shutdown();
  delete aaudio_driver;
}

}  // namespace rex::audio::android
