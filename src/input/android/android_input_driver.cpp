/**
 * @file        input/android/android_input_driver.cpp
 * @brief       Direct Android NDK / AInputEvent input driver
 *
 * @copyright   Copyright (c) 2026 ReXGlue Authors
 * @license     BSD 3-Clause License
 */

#include <rex/input/android/android_input_driver.h>

#include <algorithm>
#include <cmath>

#include <rex/logging.h>

namespace rex::input::android {

AndroidInputDriver* AndroidInputDriver::instance_ = nullptr;

AndroidInputDriver::AndroidInputDriver(rex::ui::Window* window, size_t window_z_order)
    : InputDriver(window, window_z_order) {
  instance_ = this;
}

AndroidInputDriver::~AndroidInputDriver() {
  if (instance_ == this) {
    instance_ = nullptr;
  }
}

X_STATUS AndroidInputDriver::Setup() {
  device_id_ = 1;  // Primary gamepad
  return X_STATUS_SUCCESS;
}

void AndroidInputDriver::EnumerateDevices(std::vector<DeviceInfo>& out) {
  DeviceInfo info{};
  info.id = device_id_;
  info.name = "Android Gamepad";
  info.is_synthetic = false;
  out.push_back(info);
}

X_RESULT AndroidInputDriver::GetDeviceState(DeviceId id, X_INPUT_STATE* out_state) {
  if (id != device_id_) {
    return X_ERROR_DEVICE_NOT_CONNECTED;
  }

  std::lock_guard<std::mutex> lock(state_mutex_);
  out_state->packet_number = packet_number_;
  out_state->gamepad.buttons = buttons_;
  out_state->gamepad.left_trigger = left_trigger_;
  out_state->gamepad.right_trigger = right_trigger_;
  out_state->gamepad.thumb_lx = thumb_lx_;
  out_state->gamepad.thumb_ly = thumb_ly_;
  out_state->gamepad.thumb_rx = thumb_rx_;
  out_state->gamepad.thumb_ry = thumb_ry_;

  return X_ERROR_SUCCESS;
}

X_RESULT AndroidInputDriver::GetDeviceCapabilities(DeviceId id, uint32_t /*flags*/,
                                                   X_INPUT_CAPABILITIES* out_caps) {
  if (id != device_id_) {
    return X_ERROR_DEVICE_NOT_CONNECTED;
  }

  out_caps->type = XINPUT_DEVTYPE_GAMEPAD;
  out_caps->sub_type = 1;  // Standard gamepad
  out_caps->flags = 0;
  out_caps->gamepad.buttons = 0xFFFF;
  out_caps->gamepad.left_trigger = 0xFF;
  out_caps->gamepad.right_trigger = 0xFF;
  out_caps->gamepad.thumb_lx = static_cast<int16_t>(0xFFFF);
  out_caps->gamepad.thumb_ly = static_cast<int16_t>(0xFFFF);
  out_caps->gamepad.thumb_rx = static_cast<int16_t>(0xFFFF);
  out_caps->gamepad.thumb_ry = static_cast<int16_t>(0xFFFF);
  out_caps->vibration.left_motor_speed = 0;
  out_caps->vibration.right_motor_speed = 0;

  return X_ERROR_SUCCESS;
}

X_RESULT AndroidInputDriver::SetDeviceVibration(DeviceId id, X_INPUT_VIBRATION* /*vibration*/) {
  if (id != device_id_) {
    return X_ERROR_DEVICE_NOT_CONNECTED;
  }
  // Android haptics can be wired directly here
  return X_ERROR_SUCCESS;
}

X_RESULT AndroidInputDriver::GetDeviceKeystroke(DeviceId id, uint32_t /*flags*/,
                                                X_INPUT_KEYSTROKE* /*out_keystroke*/) {
  if (id != device_id_) {
    return X_ERROR_DEVICE_NOT_CONNECTED;
  }
  return X_ERROR_EMPTY;
}

bool AndroidInputDriver::HandleAInputEvent(const AInputEvent* event) {
  int32_t event_type = AInputEvent_getType(event);
  if (event_type == AINPUT_EVENT_TYPE_KEY) {
    int32_t action = AKeyEvent_getAction(event);
    int32_t key_code = AKeyEvent_getKeyCode(event);
    HandleKeyEvent(action, key_code);
    return true;
  }
  if (event_type == AINPUT_EVENT_TYPE_MOTION) {
    HandleMotionEvent(AMOTION_EVENT_AXIS_X, AMotionEvent_getAxisValue(event, AMOTION_EVENT_AXIS_X, 0));
    HandleMotionEvent(AMOTION_EVENT_AXIS_Y, AMotionEvent_getAxisValue(event, AMOTION_EVENT_AXIS_Y, 0));
    HandleMotionEvent(AMOTION_EVENT_AXIS_Z, AMotionEvent_getAxisValue(event, AMOTION_EVENT_AXIS_Z, 0));
    HandleMotionEvent(AMOTION_EVENT_AXIS_RZ, AMotionEvent_getAxisValue(event, AMOTION_EVENT_AXIS_RZ, 0));
    HandleMotionEvent(AMOTION_EVENT_AXIS_LTRIGGER, AMotionEvent_getAxisValue(event, AMOTION_EVENT_AXIS_LTRIGGER, 0));
    HandleMotionEvent(AMOTION_EVENT_AXIS_RTRIGGER, AMotionEvent_getAxisValue(event, AMOTION_EVENT_AXIS_RTRIGGER, 0));
    HandleMotionEvent(AMOTION_EVENT_AXIS_BRAKE, AMotionEvent_getAxisValue(event, AMOTION_EVENT_AXIS_BRAKE, 0));
    HandleMotionEvent(AMOTION_EVENT_AXIS_GAS, AMotionEvent_getAxisValue(event, AMOTION_EVENT_AXIS_GAS, 0));
    HandleMotionEvent(AMOTION_EVENT_AXIS_HAT_X, AMotionEvent_getAxisValue(event, AMOTION_EVENT_AXIS_HAT_X, 0));
    HandleMotionEvent(AMOTION_EVENT_AXIS_HAT_Y, AMotionEvent_getAxisValue(event, AMOTION_EVENT_AXIS_HAT_Y, 0));
    return true;
  }
  return false;
}

void AndroidInputDriver::HandleKeyEvent(int32_t action, int32_t key_code) {
  bool pressed = (action == AKEY_EVENT_ACTION_DOWN);
  uint16_t mask = 0;

  switch (key_code) {
    case AKEYCODE_BUTTON_A:
      mask = X_INPUT_GAMEPAD_A;
      break;
    case AKEYCODE_BUTTON_B:
      mask = X_INPUT_GAMEPAD_B;
      break;
    case AKEYCODE_BUTTON_X:
      mask = X_INPUT_GAMEPAD_X;
      break;
    case AKEYCODE_BUTTON_Y:
      mask = X_INPUT_GAMEPAD_Y;
      break;
    case AKEYCODE_BUTTON_L1:
      mask = X_INPUT_GAMEPAD_LEFT_SHOULDER;
      break;
    case AKEYCODE_BUTTON_R1:
      mask = X_INPUT_GAMEPAD_RIGHT_SHOULDER;
      break;
    case AKEYCODE_BUTTON_THUMBL:
      mask = X_INPUT_GAMEPAD_LEFT_THUMB;
      break;
    case AKEYCODE_BUTTON_THUMBR:
      mask = X_INPUT_GAMEPAD_RIGHT_THUMB;
      break;
    case AKEYCODE_BUTTON_START:
      mask = X_INPUT_GAMEPAD_START;
      break;
    case AKEYCODE_BUTTON_SELECT:
    case AKEYCODE_BACK:
      mask = X_INPUT_GAMEPAD_BACK;
      break;
    case AKEYCODE_BUTTON_MODE:
      mask = X_INPUT_GAMEPAD_GUIDE;
      break;
    case AKEYCODE_DPAD_UP:
      mask = X_INPUT_GAMEPAD_DPAD_UP;
      break;
    case AKEYCODE_DPAD_DOWN:
      mask = X_INPUT_GAMEPAD_DPAD_DOWN;
      break;
    case AKEYCODE_DPAD_LEFT:
      mask = X_INPUT_GAMEPAD_DPAD_LEFT;
      break;
    case AKEYCODE_DPAD_RIGHT:
      mask = X_INPUT_GAMEPAD_DPAD_RIGHT;
      break;
    default:
      return;
  }

  SetButtonState(mask, pressed);
}

void AndroidInputDriver::HandleMotionEvent(int32_t axis, float value) {
  std::lock_guard<std::mutex> lock(state_mutex_);
  packet_number_++;

  auto scale_thumb = [](float v) -> int16_t {
    float clamped = std::clamp(v, -1.0f, 1.0f);
    if (std::abs(clamped) < 0.15f) return 0;  // Deadzone
    return static_cast<int16_t>(clamped * 32767.0f);
  };

  auto scale_trigger = [](float v) -> uint8_t {
    float clamped = std::clamp(v, 0.0f, 1.0f);
    if (clamped < 0.05f) return 0;
    return static_cast<uint8_t>(clamped * 255.0f);
  };

  switch (axis) {
    case AMOTION_EVENT_AXIS_X:
      thumb_lx_ = scale_thumb(value);
      break;
    case AMOTION_EVENT_AXIS_Y:
      thumb_ly_ = scale_thumb(-value);  // Inverted: up is positive on Xbox
      break;
    case AMOTION_EVENT_AXIS_Z:
      thumb_rx_ = scale_thumb(value);
      break;
    case AMOTION_EVENT_AXIS_RZ:
      thumb_ry_ = scale_thumb(-value);  // Inverted: up is positive on Xbox
      break;
    case AMOTION_EVENT_AXIS_LTRIGGER:
    case AMOTION_EVENT_AXIS_BRAKE:
      left_trigger_ = scale_trigger(value);
      break;
    case AMOTION_EVENT_AXIS_RTRIGGER:
    case AMOTION_EVENT_AXIS_GAS:
      right_trigger_ = scale_trigger(value);
      break;
    case AMOTION_EVENT_AXIS_HAT_X:
      if (value < -0.5f) {
        buttons_ |= X_INPUT_GAMEPAD_DPAD_LEFT;
        buttons_ &= ~X_INPUT_GAMEPAD_DPAD_RIGHT;
      } else if (value > 0.5f) {
        buttons_ |= X_INPUT_GAMEPAD_DPAD_RIGHT;
        buttons_ &= ~X_INPUT_GAMEPAD_DPAD_LEFT;
      } else {
        buttons_ &= ~(X_INPUT_GAMEPAD_DPAD_LEFT | X_INPUT_GAMEPAD_DPAD_RIGHT);
      }
      break;
    case AMOTION_EVENT_AXIS_HAT_Y:
      if (value < -0.5f) {
        buttons_ |= X_INPUT_GAMEPAD_DPAD_UP;
        buttons_ &= ~X_INPUT_GAMEPAD_DPAD_DOWN;
      } else if (value > 0.5f) {
        buttons_ |= X_INPUT_GAMEPAD_DPAD_DOWN;
        buttons_ &= ~X_INPUT_GAMEPAD_DPAD_UP;
      } else {
        buttons_ &= ~(X_INPUT_GAMEPAD_DPAD_UP | X_INPUT_GAMEPAD_DPAD_DOWN);
      }
      break;
    default:
      break;
  }
}

void AndroidInputDriver::SetButtonState(uint16_t button_mask, bool pressed) {
  std::lock_guard<std::mutex> lock(state_mutex_);
  packet_number_++;
  if (pressed) {
    buttons_ |= button_mask;
  } else {
    buttons_ &= ~button_mask;
  }
}

void AndroidInputDriver::SetTrigger(bool is_right, uint8_t value) {
  std::lock_guard<std::mutex> lock(state_mutex_);
  packet_number_++;
  if (is_right) {
    right_trigger_ = value;
  } else {
    left_trigger_ = value;
  }
}

void AndroidInputDriver::SetThumb(bool is_right, int16_t x, int16_t y) {
  std::lock_guard<std::mutex> lock(state_mutex_);
  packet_number_++;
  if (is_right) {
    thumb_rx_ = x;
    thumb_ry_ = y;
  } else {
    thumb_lx_ = x;
    thumb_ly_ = y;
  }
}

}  // namespace rex::input::android
