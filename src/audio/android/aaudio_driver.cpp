/**
 * @file        audio/android/aaudio_driver.cpp
 * @brief       Direct NDK AAudio driver with low-latency / MMAP hardware audio path
 *
 * @copyright   Copyright (c) 2026 ReXGlue Authors
 * @license     BSD 3-Clause License
 */

#include <rex/audio/android/aaudio_driver.h>

#include <algorithm>
#include <cstring>

#include <rex/assert.h>
#include <rex/audio/conversion.h>
#include <rex/audio/downmix.h>
#include <rex/audio/flags.h>
#include <rex/cvar.h>
#include <rex/dbg.h>
#include <rex/logging.h>
#include <rex/perf/counter.h>

namespace rex::audio::android {

AAudioDriver::AAudioDriver(memory::Memory* memory, rex::thread::Semaphore* semaphore)
    : AudioDriver(memory), semaphore_(semaphore) {
  ring_buffer_ = std::make_unique<AudioRingFrame[]>(kRingBufferCapacity);
  write_index_.store(0, std::memory_order_relaxed);
  read_index_.store(0, std::memory_order_relaxed);
}

AAudioDriver::~AAudioDriver() {
  Shutdown();
}

bool AAudioDriver::Initialize() {
  AAudioStreamBuilder* builder = nullptr;
  aaudio_result_t result = AAudio_createStreamBuilder(&builder);
  if (result != AAUDIO_OK) {
    REXAPU_ERROR("AAudio_createStreamBuilder failed: {}", AAudio_convertResultToText(result));
    return false;
  }

  AAudioStreamBuilder_setSampleRate(builder, kFrameFrequency);
  AAudioStreamBuilder_setFormat(builder, AAUDIO_FORMAT_PCM_FLOAT);
  AAudioStreamBuilder_setChannelCount(builder, channel_count_);
  AAudioStreamBuilder_setDirection(builder, AAUDIO_DIRECTION_OUTPUT);
  AAudioStreamBuilder_setPerformanceMode(builder, AAUDIO_PERFORMANCE_MODE_LOW_LATENCY);
  // Try EXCLUSIVE MMAP hardware path first for ultra-low latency (<3ms)
  AAudioStreamBuilder_setSharingMode(builder, AAUDIO_SHARING_MODE_EXCLUSIVE);
  AAudioStreamBuilder_setUsage(builder, AAUDIO_USAGE_GAME);
  AAudioStreamBuilder_setContentType(builder, AAUDIO_CONTENT_TYPE_MUSIC);
  AAudioStreamBuilder_setDataCallback(builder, DataCallback, this);
  AAudioStreamBuilder_setErrorCallback(builder, ErrorCallback, this);

  result = AAudioStreamBuilder_openStream(builder, &aaudio_stream_);
  if (result != AAUDIO_OK || !aaudio_stream_) {
    // Fallback to SHARED mode if device does not grant exclusive access
    AAudioStreamBuilder_setSharingMode(builder, AAUDIO_SHARING_MODE_SHARED);
    result = AAudioStreamBuilder_openStream(builder, &aaudio_stream_);
  }
  AAudioStreamBuilder_delete(builder);

  if (result != AAUDIO_OK || !aaudio_stream_) {
    REXAPU_ERROR("AAudioStreamBuilder_openStream failed: {}", AAudio_convertResultToText(result));
    return false;
  }

  // Tune buffer size to double burst size for optimum DSP hardware sync
  int32_t burst_frames = AAudioStream_getFramesPerBurst(aaudio_stream_);
  if (burst_frames > 0) {
    AAudioStream_setBufferSizeInFrames(aaudio_stream_, burst_frames * 2);
  }

  channel_count_ = AAudioStream_getChannelCount(aaudio_stream_);
  int32_t sample_rate = AAudioStream_getSampleRate(aaudio_stream_);
  aaudio_sharing_mode_t sharing_mode = AAudioStream_getSharingMode(aaudio_stream_);
  aaudio_performance_mode_t perf_mode = AAudioStream_getPerformanceMode(aaudio_stream_);

  REXAPU_INFO("AAudio endpoint opened: {} ch, {} Hz, sharing={}, perf={}, burst={}",
              channel_count_, sample_rate,
              sharing_mode == AAUDIO_SHARING_MODE_EXCLUSIVE ? "EXCLUSIVE(MMAP)" : "SHARED",
              perf_mode == AAUDIO_PERFORMANCE_MODE_LOW_LATENCY ? "LOW_LATENCY" : "NORMAL",
              burst_frames);

  result = AAudioStream_requestStart(aaudio_stream_);
  if (result != AAUDIO_OK) {
    REXAPU_ERROR("AAudioStream_requestStart failed: {}", AAudio_convertResultToText(result));
    AAudioStream_close(aaudio_stream_);
    aaudio_stream_ = nullptr;
    return false;
  }

  return true;
}

void AAudioDriver::SubmitFrame(uint32_t frame_ptr) {
  const auto input_frame = memory_->TranslateVirtual<float*>(frame_ptr);
  if (!ring_buffer_) return;

  const size_t current_write = write_index_.load(std::memory_order_relaxed);
  const size_t current_read = read_index_.load(std::memory_order_acquire);

  if ((current_write - current_read) >= kRingBufferCapacity) {
    // Ring buffer overflow: advance read to keep audio fresh and avoid drift
    read_index_.store(current_read + 1, std::memory_order_release);
  }

  const size_t slot = current_write % kRingBufferCapacity;
  std::memcpy(ring_buffer_[slot].samples, input_frame, kGuestFrameSize);
  write_index_.store(current_write + 1, std::memory_order_release);

  PROFILE_BUFFER_QUEUE_DEPTH(static_cast<int64_t>(current_write - current_read));
}

void AAudioDriver::Shutdown() {
  if (aaudio_stream_) {
    AAudioStream_requestStop(aaudio_stream_);
    AAudioStream_close(aaudio_stream_);
    aaudio_stream_ = nullptr;
  }

  write_index_.store(0, std::memory_order_release);
  read_index_.store(0, std::memory_order_release);
  leftover_buffer_.clear();
  leftover_offset_ = 0;
}

aaudio_data_callback_result_t AAudioDriver::DataCallback(
    AAudioStream* /*stream*/, void* user_data, void* audio_data, int32_t num_frames) {
  SCOPE_profile_cpu_f("apu");
  if (!user_data || !audio_data || num_frames <= 0) {
    return AAUDIO_CALLBACK_RESULT_CONTINUE;
  }

  auto* driver = static_cast<AAudioDriver*>(user_data);
  auto* out_samples = static_cast<float*>(audio_data);
  const int32_t channels = driver->channel_count_;
  const StereoFold fold = GetStereoFold();
  const SurroundMix mix = GetSurroundMix();
  const float gain = GetOutputGain();
  const bool muted = REXCVAR_GET(audio_mute);

  int32_t frames_needed = num_frames;

  // Drain leftover buffer first if any exists from previous callback
  if (!driver->leftover_buffer_.empty()) {
    size_t available_leftover_frames = (driver->leftover_buffer_.size() - driver->leftover_offset_) / channels;
    size_t frames_to_copy = std::min<size_t>(frames_needed, available_leftover_frames);
    std::memcpy(out_samples,
                driver->leftover_buffer_.data() + driver->leftover_offset_,
                frames_to_copy * channels * sizeof(float));
    out_samples += frames_to_copy * channels;
    driver->leftover_offset_ += frames_to_copy * channels;
    frames_needed -= static_cast<int32_t>(frames_to_copy);

    if (driver->leftover_offset_ >= driver->leftover_buffer_.size()) {
      driver->leftover_buffer_.clear();
      driver->leftover_offset_ = 0;
    }
  }

  // Temporary buffer for converting a 256-sample guest frame into interleaved device channels
  float converted_frame[kChannelSamples * 6];

  while (frames_needed > 0) {
    float* guest_buffer = nullptr;
    const size_t current_read = driver->read_index_.load(std::memory_order_relaxed);
    const size_t current_write = driver->write_index_.load(std::memory_order_acquire);

    if (current_read < current_write) {
      const size_t slot = current_read % kRingBufferCapacity;
      guest_buffer = driver->ring_buffer_[slot].samples;
    }

    if (!guest_buffer) {
      // Silence underflow
      std::memset(out_samples, 0, frames_needed * channels * sizeof(float));
      break;
    }

    if (muted) {
      std::memset(converted_frame, 0, kChannelSamples * channels * sizeof(float));
    } else if (channels <= 2) {
      conversion::sequential_6_BE_to_interleaved_2_LE(
          converted_frame, guest_buffer, kChannelSamples, fold, gain);
    } else {
      conversion::sequential_6_BE_to_interleaved_6_LE(
          converted_frame, guest_buffer, kChannelSamples, mix, gain);
    }

    // Advance consumer pointer lock-free
    driver->read_index_.store(current_read + 1, std::memory_order_release);
    if (driver->semaphore_) {
      driver->semaphore_->Release(1, nullptr);
    }

    int32_t frames_to_copy = std::min<int32_t>(frames_needed, kChannelSamples);
    std::memcpy(out_samples, converted_frame, frames_to_copy * channels * sizeof(float));
    out_samples += frames_to_copy * channels;
    frames_needed -= frames_to_copy;

    // Stash remaining converted frames if the hardware requested fewer than kChannelSamples
    if (frames_to_copy < static_cast<int32_t>(kChannelSamples)) {
      size_t remaining_samples = (kChannelSamples - frames_to_copy) * channels;
      driver->leftover_buffer_.resize(remaining_samples);
      std::memcpy(driver->leftover_buffer_.data(),
                  converted_frame + (frames_to_copy * channels),
                  remaining_samples * sizeof(float));
      driver->leftover_offset_ = 0;
    }
  }

  return AAUDIO_CALLBACK_RESULT_CONTINUE;
}

void AAudioDriver::ErrorCallback(AAudioStream* /*stream*/, void* /*user_data*/, aaudio_result_t error) {
  REXAPU_WARN("AAudio stream error callback: {}", AAudio_convertResultToText(error));
}

}  // namespace rex::audio::android
