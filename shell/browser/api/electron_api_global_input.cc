// Copyright (c) 2014 GitHub, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#include "shell/browser/api/electron_api_global_input.h"

#include "content/public/browser/desktop_media_id.h"
#include "gin/dictionary.h"
#include "gin/object_template_builder.h"
#include "shell/browser/browser.h"
#include "shell/browser/javascript_environment.h"
#include "shell/common/gin_helper/error_thrower.h"
#include "shell/common/node_includes.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"

#if BUILDFLAG(IS_WIN)
#include "third_party/webrtc/modules/desktop_capture/win/screen_capture_utils.h"
#include "third_party/webrtc/modules/desktop_capture/win/window_capture_utils.h"
#endif

#if BUILDFLAG(IS_MAC)
#include "third_party/webrtc/modules/desktop_capture/mac/desktop_configuration.h"
#include "third_party/webrtc/modules/desktop_capture/mac/window_list_utils.h"

#include <Carbon/Carbon.h>
#include "base/mac/scoped_cftyperef.h"
#endif

namespace {

bool GetLocalPos(const std::string& id,
                 int remote_x,
                 int remote_y,
                 int view_width,
                 int view_height,
                 int& local_x,
                 int& local_y) {
  // parse desktop share source id
  const content::DesktopMediaID source = content::DesktopMediaID::Parse(id);
  if (source.is_null()) {
    LOG(ERROR) << "SendMouse: Desktop media ID is null";
    return false;
  }

  // get local rect
  webrtc::DesktopRect local_rect;
  switch (source.type) {
    case content::DesktopMediaID::TYPE_SCREEN: {
#if BUILDFLAG(IS_WIN)
      std::wstring device_key;
      if (!webrtc::IsScreenValid(source.id, &device_key)) {
        LOG(ERROR) << "SendMouse: Desktop media ID is invalid";
        return false;
      }
      local_rect = webrtc::GetScreenRect(source.id, device_key);
#elif BUILDFLAG(IS_MAC)
      webrtc::MacDesktopConfiguration desktop_config =
          webrtc::MacDesktopConfiguration::GetCurrent(
              webrtc::MacDesktopConfiguration::TopLeftOrigin);
      const webrtc::MacDisplayConfiguration* config =
          desktop_config.FindDisplayConfigurationById(
              static_cast<CGDirectDisplayID>(source.id));
      if (!config) {
        LOG(ERROR) << "SendMouse: Desktop media ID is invalid";
        return false;
      }
      local_rect = config->bounds;
#else
      LOG(WARNING) << "SendMouse not supported";
#endif
      break;
    }

    case content::DesktopMediaID::TYPE_WINDOW: {
#if BUILDFLAG(IS_WIN)
      if (!webrtc::GetWindowRect(reinterpret_cast<HWND>(source.id),
                                 &local_rect)) {
        LOG(ERROR) << "SendMouse: GetWindowRect failed";
        return false;
      }
#elif BUILDFLAG(IS_MAC)
      local_rect = webrtc::GetWindowBounds(source.id);
#else
      LOG(WARNING) << "SendMouse not supported";
#endif
      break;
    }

    default:
      LOG(ERROR) << "SendMouse: Desktop media type is invalid";
      return false;
  }

  if (local_rect.is_empty()) {
    LOG(ERROR) << "SendMouse: local rect is invalid";
    return false;
  }

  // calc remote rect
  int remote_width, remote_height;
  if (local_rect.width() * view_height > local_rect.height() * view_width) {
    remote_width = view_width;
    remote_height = local_rect.height() * remote_width / local_rect.width();
    CHECK_LE(remote_height, view_height);
  } else {
    remote_height = view_height;
    remote_width = local_rect.width() * remote_height / local_rect.height();
    CHECK_LE(remote_width, view_width);
  }
  int remote_left = (view_width - remote_width) / 2;
  int remote_top = (view_height - remote_height) / 2;
  CHECK_GE(remote_left, 0);
  CHECK_GE(remote_top, 0);
  LOG(INFO) << "SendMouse: remote rect: (" << remote_left << "," << remote_top
            << ")-(" << remote_width << "," << remote_height << ")";
  webrtc::DesktopRect remote_rect = webrtc::DesktopRect::MakeXYWH(
      remote_left, remote_top, remote_width, remote_height);
  webrtc::DesktopVector remote_pos(remote_x, remote_y);
  if (!remote_rect.Contains(remote_pos)) {
    LOG(INFO) << "SendMouse: remote pos is out of remote rect";
    return false;
  }

  // calc local pos
  local_x = (remote_x - remote_rect.left()) * local_rect.width() /
                remote_rect.width() +
            local_rect.left();
  local_y = (remote_y - remote_rect.top()) * local_rect.height() /
                remote_rect.height() +
            local_rect.top();
  webrtc::DesktopVector local_pos(local_x, local_y);
  if (!local_rect.Contains(local_pos)) {
    LOG(INFO) << "SendMouse: local pos is out of local rect";
    return false;
  }

  return true;
}

void FixWheelDelta(int& dx, int& dy) {
  if (dx > 0) {
    dx = 1;
  } else if (dx < 0) {
    dx = -1;
  }
  if (dy > 0) {
    dy = 1;
  } else if (dy < 0) {
    dy = -1;
  }
  if (abs(dx) > abs(dy)) {
    dy = 0;
  } else {
    dx = 0;
  }
}

#if BUILDFLAG(IS_WIN)
void HandleMouseMove(int point_x, int point_y) {
  INPUT input;
  ZeroMemory(&input, sizeof(INPUT));
  input.type = INPUT_MOUSE;
  input.mi.dwFlags =
      MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_VIRTUALDESK;
  input.mi.dx = (point_x - GetSystemMetrics(SM_XVIRTUALSCREEN)) * 65535 /
                GetSystemMetrics(SM_CXVIRTUALSCREEN);
  input.mi.dy = (point_y - GetSystemMetrics(SM_YVIRTUALSCREEN)) * 65535 /
                GetSystemMetrics(SM_CYVIRTUALSCREEN);
  ::SendInput(1, &input, sizeof(INPUT));
}

void HandleMouseDown(int buttons) {
  INPUT input;
  ZeroMemory(&input, sizeof(INPUT));
  input.type = INPUT_MOUSE;
  switch (buttons) {
    case 0x01:
      input.mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
      break;
    case 0x02:
      input.mi.dwFlags = MOUSEEVENTF_RIGHTDOWN;
      break;
    case 0x04:
      input.mi.dwFlags = MOUSEEVENTF_MIDDLEDOWN;
      break;
    case 0x08:
      input.mi.dwFlags = MOUSEEVENTF_XDOWN;
      input.mi.mouseData = XBUTTON1;
      break;
    case 0x10:
      input.mi.dwFlags = MOUSEEVENTF_XDOWN;
      input.mi.mouseData = XBUTTON2;
      break;
    default:
      LOG(INFO) << "HandleMouseDown: unsupported button";
      return;
  }
  ::SendInput(1, &input, sizeof(INPUT));
}

void HandleMouseUp(int buttons) {
  INPUT input;
  ZeroMemory(&input, sizeof(INPUT));
  input.type = INPUT_MOUSE;
  switch (buttons) {
    case 0x01:
      input.mi.dwFlags = MOUSEEVENTF_LEFTUP;
      break;
    case 0x02:
      input.mi.dwFlags = MOUSEEVENTF_RIGHTUP;
      break;
    case 0x04:
      input.mi.dwFlags = MOUSEEVENTF_MIDDLEUP;
      break;
    case 0x08:
      input.mi.dwFlags = MOUSEEVENTF_XUP;
      input.mi.mouseData = XBUTTON1;
      break;
    case 0x10:
      input.mi.dwFlags = MOUSEEVENTF_XUP;
      input.mi.mouseData = XBUTTON2;
      break;
    default:
      LOG(INFO) << "HandleMouseUp: unsupported button";
      return;
  }
  ::SendInput(1, &input, sizeof(INPUT));
}

void HandleWheel(int delta_x, int delta_y) {
  INPUT input;
  ZeroMemory(&input, sizeof(INPUT));
  input.type = INPUT_MOUSE;
  if (delta_x != 0) {
    input.mi.dwFlags = MOUSEEVENTF_HWHEEL;
    input.mi.mouseData = delta_x * WHEEL_DELTA;
    ::SendInput(1, &input, sizeof(INPUT));
  }
  if (delta_y != 0) {
    input.mi.dwFlags = MOUSEEVENTF_WHEEL;
    input.mi.mouseData = delta_y * WHEEL_DELTA;
    ::SendInput(1, &input, sizeof(INPUT));
  }
}
#elif BUILDFLAG(IS_MAC)
CGEventFlags GetModifiers(bool alt, bool ctrl, bool shift, bool meta) {
  CGEventFlags flags = 0;
  if (alt) {
    flags |= kCGEventFlagMaskAlternate;
  }
  if (ctrl) {
    flags |= kCGEventFlagMaskControl;
  }
  if (shift) {
    flags |= kCGEventFlagMaskShift;
  }
  if (meta) {
    flags |= kCGEventFlagMaskCommand;
  }
  return flags;
}

void HandleMouseMove(int point_x,
                     int point_y,
                     int buttons,
                     CGEventFlags flags) {
  CGPoint location = CGPointMake(point_x, point_y);
  base::ScopedCFTypeRef<CGEventSourceRef> event_source(
      CGEventSourceCreate(kCGEventSourceStateHIDSystemState));
  base::ScopedCFTypeRef<CGEventRef> mouse_event;
  switch (buttons) {
    case 0x01:
      mouse_event.reset(CGEventCreateMouseEvent(
          event_source, CGEventType::kCGEventLeftMouseDragged, location,
          CGMouseButton::kCGMouseButtonLeft));
      break;
    case 0x02:
      mouse_event.reset(CGEventCreateMouseEvent(
          event_source, CGEventType::kCGEventRightMouseDragged, location,
          CGMouseButton::kCGMouseButtonRight));
      break;
    case 0x04:
      mouse_event.reset(CGEventCreateMouseEvent(
          event_source, CGEventType::kCGEventOtherMouseDragged, location,
          CGMouseButton::kCGMouseButtonCenter));
      break;
    default:
      mouse_event.reset(
          CGEventCreateMouseEvent(event_source, CGEventType::kCGEventMouseMoved,
                                  location, CGMouseButton::kCGMouseButtonLeft));
      break;
  }
  CGEventSetFlags(mouse_event, flags);
  CGEventPost(kCGHIDEventTap, mouse_event);
}

void HandleMouseDown(int point_x,
                     int point_y,
                     int buttons,
                     CGEventFlags flags) {
  CGPoint location = CGPointMake(point_x, point_y);
  base::ScopedCFTypeRef<CGEventSourceRef> event_source(
      CGEventSourceCreate(kCGEventSourceStateHIDSystemState));
  base::ScopedCFTypeRef<CGEventRef> mouse_event;
  switch (buttons) {
    case 0x01:
      mouse_event.reset(CGEventCreateMouseEvent(
          event_source, CGEventType::kCGEventLeftMouseDown, location,
          CGMouseButton::kCGMouseButtonLeft));
      break;
    case 0x02:
      mouse_event.reset(CGEventCreateMouseEvent(
          event_source, CGEventType::kCGEventRightMouseDown, location,
          CGMouseButton::kCGMouseButtonRight));
      break;
    case 0x04:
      mouse_event.reset(CGEventCreateMouseEvent(
          event_source, CGEventType::kCGEventOtherMouseDown, location,
          CGMouseButton::kCGMouseButtonCenter));
      break;
    default:
      LOG(INFO) << "HandleMouseDown: unsupported button";
      return;
  }
  CGEventSetFlags(mouse_event, flags);
  CGEventPost(kCGHIDEventTap, mouse_event);
}

void HandleMouseUp(int point_x, int point_y, int buttons, CGEventFlags flags) {
  CGPoint location = CGPointMake(point_x, point_y);
  base::ScopedCFTypeRef<CGEventSourceRef> event_source(
      CGEventSourceCreate(kCGEventSourceStateHIDSystemState));
  base::ScopedCFTypeRef<CGEventRef> mouse_event;
  switch (buttons) {
    case 0x01:
      mouse_event.reset(CGEventCreateMouseEvent(
          event_source, CGEventType::kCGEventLeftMouseUp, location,
          CGMouseButton::kCGMouseButtonLeft));
      break;
    case 0x02:
      mouse_event.reset(CGEventCreateMouseEvent(
          event_source, CGEventType::kCGEventRightMouseUp, location,
          CGMouseButton::kCGMouseButtonRight));
      break;
    case 0x04:
      mouse_event.reset(CGEventCreateMouseEvent(
          event_source, CGEventType::kCGEventOtherMouseUp, location,
          CGMouseButton::kCGMouseButtonCenter));
      break;
    default:
      LOG(INFO) << "HandleMouseUp: unsupported button";
      return;
  }
  CGEventSetFlags(mouse_event, flags);
  CGEventPost(kCGHIDEventTap, mouse_event);
}

void HandleWheel(int delta_x, int delta_y, CGEventFlags flags) {
  base::ScopedCFTypeRef<CGEventSourceRef> event_source(
      CGEventSourceCreate(kCGEventSourceStateHIDSystemState));
  base::ScopedCFTypeRef<CGEventRef> wheel_event;
  if (delta_x != 0) {
    wheel_event.reset(CGEventCreateScrollWheelEvent(
        event_source, kCGScrollEventUnitLine, 2, 0, -delta_x));
    CGEventSetFlags(wheel_event, flags);
    CGEventPost(kCGHIDEventTap, wheel_event);
  }
  if (delta_y != 0) {
    wheel_event.reset(CGEventCreateScrollWheelEvent(
        event_source, kCGScrollEventUnitLine, 1, -delta_y));
    CGEventSetFlags(wheel_event, flags);
    CGEventPost(kCGHIDEventTap, wheel_event);
  }
}
#endif

}  // namespace

namespace electron::api {

gin::WrapperInfo GlobalInput::kWrapperInfo = {gin::kEmbedderNativeGin};

GlobalInput::GlobalInput(v8::Isolate* isolate) {}

GlobalInput::~GlobalInput() {}

bool GlobalInput::SendMouse(const std::string& id,
                            int action,
                            int buttons,
                            int dx,
                            int dy,
                            int x,
                            int y,
                            int width,
                            int height,
                            bool alt,
                            bool ctrl,
                            bool shift,
                            bool meta) {
  if (!electron::Browser::Get()->is_ready()) {
    gin_helper::ErrorThrower(JavascriptEnvironment::GetIsolate())
        .ThrowError("globalInput cannot be used before the app is ready");
    return false;
  }

  if (action != 0) {
    LOG(INFO) << "SendMouse: [" << id << "] [" << action << ":" << buttons
              << "] {" << dx << "," << dy << "} (" << x << "," << y << ")-("
              << width << "," << height << ") alt:" << alt << " ctrl:" << ctrl
              << " shift:" << shift << " meta:" << meta;
  }

  int xPos = 0;
  int yPos = 0;
  if (!GetLocalPos(id, x, y, width, height, xPos, yPos)) {
    return false;
  }

#if BUILDFLAG(IS_MAC)
  CGEventFlags flags = GetModifiers(alt, ctrl, shift, meta);
#endif

  switch (action) {
    case 0:  // move
#if BUILDFLAG(IS_WIN)
      HandleMouseMove(xPos, yPos);
#elif BUILDFLAG(IS_MAC)
      HandleMouseMove(xPos, yPos, buttons, flags);
#endif
      break;
    case 1:  // down
#if BUILDFLAG(IS_WIN)
      HandleMouseDown(buttons);
#elif BUILDFLAG(IS_MAC)
      HandleMouseDown(xPos, yPos, buttons, flags);
#endif
      break;
    case 2:  // up
#if BUILDFLAG(IS_WIN)
      HandleMouseUp(buttons);
#elif BUILDFLAG(IS_MAC)
      HandleMouseUp(xPos, yPos, buttons, flags);
#endif
      break;
    case 3:  // wheel
      FixWheelDelta(dx, dy);
#if BUILDFLAG(IS_WIN)
      HandleWheel(dx, dy);
#elif BUILDFLAG(IS_MAC)
      HandleWheel(dx, dy, flags);
#endif
      break;
    case 4:  // trackpad
      LOG(INFO) << "SendMouse: unsupported trackpad";
      return false;
    default:
      LOG(INFO) << "SendMouse: unsupported action";
      return false;
  }

  return true;
}

bool GlobalInput::SendKeyboard(bool down,
                               int vkey,
                               bool alt,
                               bool ctrl,
                               bool shift,
                               bool meta) {
  if (!electron::Browser::Get()->is_ready()) {
    gin_helper::ErrorThrower(JavascriptEnvironment::GetIsolate())
        .ThrowError("globalInput cannot be used before the app is ready");
    return false;
  }

  LOG(INFO) << "SendKeyboard: [" << down << ":" << vkey << "] alt:" << alt
            << " ctrl:" << ctrl << " shift:" << shift << " meta:" << meta;

#if BUILDFLAG(IS_WIN)
#define MAX_INPUT_KEYS 5
  INPUT inputs[MAX_INPUT_KEYS] = {};
  ZeroMemory(&inputs, sizeof(inputs));
  UINT i = 0;

  if (vkey >= 0) {
    inputs[i].type = INPUT_KEYBOARD;
    inputs[i].ki.wVk = vkey;
    if (!down) {
      inputs[i].ki.dwFlags = KEYEVENTF_KEYUP;
    }
    i++;
  }
  // if (alt) {
  //   inputs[i].type = INPUT_KEYBOARD;
  //   inputs[i].ki.wVk = VK_MENU;
  //   if (!down) {
  //     inputs[i].ki.dwFlags = KEYEVENTF_KEYUP;
  //   }
  //   i++;
  // }
  // if (ctrl) {
  //   inputs[i].type = INPUT_KEYBOARD;
  //   inputs[i].ki.wVk = VK_CONTROL;
  //   if (!down) {
  //     inputs[i].ki.dwFlags = KEYEVENTF_KEYUP;
  //   }
  //   i++;
  // }
  // if (shift) {
  //   inputs[i].type = INPUT_KEYBOARD;
  //   inputs[i].ki.wVk = VK_SHIFT;
  //   if (!down) {
  //     inputs[i].ki.dwFlags = KEYEVENTF_KEYUP;
  //   }
  //   i++;
  // }
  // if (meta) {
  //   inputs[i].type = INPUT_KEYBOARD;
  //   inputs[i].ki.wVk = VK_LWIN;
  //   if (!down) {
  //     inputs[i].ki.dwFlags = KEYEVENTF_KEYUP;
  //   }
  //   i++;
  // }

  CHECK_LE(i, MAX_INPUT_KEYS);
  if (i != ::SendInput(i, inputs, sizeof(INPUT))) {
    LOG(ERROR) << "SendKeyboard: SendInput failed";
    return false;
  }

  return true;
#elif BUILDFLAG(IS_MAC)
  CGEventFlags flags = GetModifiers(alt, ctrl, shift, meta);
  base::ScopedCFTypeRef<CGEventSourceRef> event_source(
      CGEventSourceCreate(kCGEventSourceStateHIDSystemState));
  base::ScopedCFTypeRef<CGEventRef> key_event(
      CGEventCreateKeyboardEvent(event_source, vkey, down));
  CGEventSetFlags(key_event, flags);
  CGEventPost(kCGHIDEventTap, key_event);
  return true;
#else
  LOG(WARNING) << "SendKeyboard not supported";
  return false;
#endif
}

// static
gin::Handle<GlobalInput> GlobalInput::Create(v8::Isolate* isolate) {
  return gin::CreateHandle(isolate, new GlobalInput(isolate));
}

// static
gin::ObjectTemplateBuilder GlobalInput::GetObjectTemplateBuilder(
    v8::Isolate* isolate) {
  return gin::Wrappable<GlobalInput>::GetObjectTemplateBuilder(isolate)
      .SetMethod("sendMouse", &GlobalInput::SendMouse)
      .SetMethod("sendKeyboard", &GlobalInput::SendKeyboard);
}

const char* GlobalInput::GetTypeName() {
  return "GlobalInput";
}

}  // namespace electron::api

namespace {

void Initialize(v8::Local<v8::Object> exports,
                v8::Local<v8::Value> unused,
                v8::Local<v8::Context> context,
                void* priv) {
  v8::Isolate* isolate = context->GetIsolate();
  gin::Dictionary dict(isolate, exports);
  dict.Set("globalInput", electron::api::GlobalInput::Create(isolate));
}

}  // namespace

NODE_LINKED_MODULE_CONTEXT_AWARE(electron_browser_global_input, Initialize)
