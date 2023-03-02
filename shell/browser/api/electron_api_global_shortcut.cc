// Copyright (c) 2014 GitHub, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#include "shell/browser/api/electron_api_global_shortcut.h"

#include <vector>

#include "base/containers/contains.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "extensions/common/command.h"
#include "gin/dictionary.h"
#include "gin/object_template_builder.h"
#include "shell/browser/api/electron_api_system_preferences.h"
#include "shell/browser/browser.h"
#include "shell/common/gin_converters/accelerator_converter.h"
#include "shell/common/gin_converters/callback_converter.h"
#include "shell/common/node_includes.h"

#if BUILDFLAG(IS_MAC)
#include <Carbon/Carbon.h>
#include "base/mac/mac_util.h"
#include "base/mac/scoped_cftyperef.h"
#endif

using extensions::Command;
using extensions::GlobalShortcutListener;

namespace {

#if BUILDFLAG(IS_MAC)
bool RegisteringMediaKeyForUntrustedClient(const ui::Accelerator& accelerator) {
  if (base::mac::IsAtLeastOS10_14()) {
    if (Command::IsMediaKey(accelerator)) {
      if (!electron::api::SystemPreferences::IsTrustedAccessibilityClient(
              false))
        return true;
    }
  }
  return false;
}

bool MapHasMediaKeys(
    const std::map<ui::Accelerator, base::RepeatingClosure>& accelerator_map) {
  auto media_key = std::find_if(
      accelerator_map.begin(), accelerator_map.end(),
      [](const auto& ac) { return Command::IsMediaKey(ac.first); });

  return media_key != accelerator_map.end();
}
#endif

}  // namespace

namespace electron::api {

gin::WrapperInfo GlobalShortcut::kWrapperInfo = {gin::kEmbedderNativeGin};

GlobalShortcut::GlobalShortcut(v8::Isolate* isolate) {}

GlobalShortcut::~GlobalShortcut() {
  UnregisterAll();
}

void GlobalShortcut::OnKeyPressed(const ui::Accelerator& accelerator) {
  if (accelerator_callback_map_.find(accelerator) ==
      accelerator_callback_map_.end()) {
    // This should never occur, because if it does, GlobalShortcutListener
    // notifies us with wrong accelerator.
    NOTREACHED();
    return;
  }
  accelerator_callback_map_[accelerator].Run();
}

#if BUILDFLAG(IS_MAC)
static void SendGlobalKeyEvent(int key_code,
                               CGEventFlags flags,
                               bool key_down) {
  base::ScopedCFTypeRef<CGEventSourceRef> event_source(
      CGEventSourceCreate(kCGEventSourceStateHIDSystemState));
  base::ScopedCFTypeRef<CGEventRef> key_event(
      CGEventCreateKeyboardEvent(event_source, key_code, key_down));
  CGEventSetFlags(key_event, flags);
  CGEventPost(kCGHIDEventTap, key_event);
}
#endif

bool GlobalShortcut::Send(const ui::Accelerator& accelerator) {
  if (!electron::Browser::Get()->is_ready()) {
    gin_helper::ErrorThrower(JavascriptEnvironment::GetIsolate())
        .ThrowError("globalShortcut cannot be used before the app is ready");
    return false;
  }

  if (accelerator.IsCtrlDown()) {
    LOG(INFO) << "GlobalShortcut::Send: accelerator control is down";
  }
  if (accelerator.IsCmdDown()) {
    LOG(INFO) << "GlobalShortcut::Send: accelerator command is down";
  }
  LOG(INFO) << "GlobalShortcut::Send: accelerator key_code is "
            << accelerator.key_code();

#if BUILDFLAG(IS_WIN)
  // Clear Shift if needed.
  if (GetAsyncKeyState(VK_SHIFT) & 0x8000) {
    LOG(INFO) << "GlobalShortcut::Send: Shift is down";
    INPUT input;
    ZeroMemory(&input, sizeof(input));
    input.type = INPUT_KEYBOARD;
    input.ki.wVk = VK_SHIFT;
    input.ki.dwFlags = KEYEVENTF_KEYUP;
    if (1 != ::SendInput(1, &input, sizeof(INPUT))) {
      LOG(ERROR) << "GlobalShortcut::Send: Shift up failed";
      return false;
    }
  }

  // Send Ctrl+C.
  INPUT inputs[4] = {};
  ZeroMemory(inputs, sizeof(inputs));

  inputs[0].type = INPUT_KEYBOARD;
  inputs[0].ki.wVk = VK_CONTROL;

  inputs[1].type = INPUT_KEYBOARD;
  inputs[1].ki.wVk = 'C';

  inputs[2].type = INPUT_KEYBOARD;
  inputs[2].ki.wVk = 'C';
  inputs[2].ki.dwFlags = KEYEVENTF_KEYUP;

  inputs[3].type = INPUT_KEYBOARD;
  inputs[3].ki.wVk = VK_CONTROL;
  inputs[3].ki.dwFlags = KEYEVENTF_KEYUP;

  if (4 != ::SendInput(4, inputs, sizeof(INPUT))) {
    LOG(ERROR) << "GlobalShortcut::Send: Ctrl+C failed";
    return false;
  }

  return true;
#elif BUILDFLAG(IS_MAC)
  // Send Ctrl+C.
  SendGlobalKeyEvent(kVK_ANSI_C, kCGEventFlagMaskCommand, true);
  SendGlobalKeyEvent(kVK_ANSI_C, kCGEventFlagMaskCommand, false);
  return true;
#else
  LOG(WARNING) << "GlobalShortcut::Send: not supported";
  return false;
#endif
}

bool GlobalShortcut::RegisterAll(
    const std::vector<ui::Accelerator>& accelerators,
    const base::RepeatingClosure& callback) {
  if (!electron::Browser::Get()->is_ready()) {
    gin_helper::ErrorThrower(JavascriptEnvironment::GetIsolate())
        .ThrowError("globalShortcut cannot be used before the app is ready");
    return false;
  }
  std::vector<ui::Accelerator> registered;

  for (auto& accelerator : accelerators) {
    if (!Register(accelerator, callback)) {
      // Unregister all shortcuts if any failed.
      UnregisterSome(registered);
      return false;
    }

    registered.push_back(accelerator);
  }
  return true;
}

bool GlobalShortcut::Register(const ui::Accelerator& accelerator,
                              const base::RepeatingClosure& callback) {
  if (!electron::Browser::Get()->is_ready()) {
    gin_helper::ErrorThrower(JavascriptEnvironment::GetIsolate())
        .ThrowError("globalShortcut cannot be used before the app is ready");
    return false;
  }
#if BUILDFLAG(IS_MAC)
  if (Command::IsMediaKey(accelerator)) {
    if (RegisteringMediaKeyForUntrustedClient(accelerator))
      return false;

    GlobalShortcutListener::SetShouldUseInternalMediaKeyHandling(false);
  }
#endif

  if (!GlobalShortcutListener::GetInstance()->RegisterAccelerator(accelerator,
                                                                  this)) {
    return false;
  }

  accelerator_callback_map_[accelerator] = callback;
  return true;
}

void GlobalShortcut::Unregister(const ui::Accelerator& accelerator) {
  if (!electron::Browser::Get()->is_ready()) {
    gin_helper::ErrorThrower(JavascriptEnvironment::GetIsolate())
        .ThrowError("globalShortcut cannot be used before the app is ready");
    return;
  }
  if (accelerator_callback_map_.erase(accelerator) == 0)
    return;

#if BUILDFLAG(IS_MAC)
  if (Command::IsMediaKey(accelerator) &&
      !MapHasMediaKeys(accelerator_callback_map_)) {
    GlobalShortcutListener::SetShouldUseInternalMediaKeyHandling(true);
  }
#endif

  GlobalShortcutListener::GetInstance()->UnregisterAccelerator(accelerator,
                                                               this);
}

void GlobalShortcut::UnregisterSome(
    const std::vector<ui::Accelerator>& accelerators) {
  for (auto& accelerator : accelerators) {
    Unregister(accelerator);
  }
}

bool GlobalShortcut::IsRegistered(const ui::Accelerator& accelerator) {
  return base::Contains(accelerator_callback_map_, accelerator);
}

void GlobalShortcut::UnregisterAll() {
  if (!electron::Browser::Get()->is_ready()) {
    gin_helper::ErrorThrower(JavascriptEnvironment::GetIsolate())
        .ThrowError("globalShortcut cannot be used before the app is ready");
    return;
  }
  accelerator_callback_map_.clear();
  GlobalShortcutListener::GetInstance()->UnregisterAccelerators(this);
}

// static
gin::Handle<GlobalShortcut> GlobalShortcut::Create(v8::Isolate* isolate) {
  return gin::CreateHandle(isolate, new GlobalShortcut(isolate));
}

// static
gin::ObjectTemplateBuilder GlobalShortcut::GetObjectTemplateBuilder(
    v8::Isolate* isolate) {
  return gin::Wrappable<GlobalShortcut>::GetObjectTemplateBuilder(isolate)
      .SetMethod("send", &GlobalShortcut::Send)
      .SetMethod("registerAll", &GlobalShortcut::RegisterAll)
      .SetMethod("register", &GlobalShortcut::Register)
      .SetMethod("isRegistered", &GlobalShortcut::IsRegistered)
      .SetMethod("unregister", &GlobalShortcut::Unregister)
      .SetMethod("unregisterAll", &GlobalShortcut::UnregisterAll);
}

const char* GlobalShortcut::GetTypeName() {
  return "GlobalShortcut";
}

}  // namespace electron::api

namespace {

void Initialize(v8::Local<v8::Object> exports,
                v8::Local<v8::Value> unused,
                v8::Local<v8::Context> context,
                void* priv) {
  v8::Isolate* isolate = context->GetIsolate();
  gin::Dictionary dict(isolate, exports);
  dict.Set("globalShortcut", electron::api::GlobalShortcut::Create(isolate));
}

}  // namespace

NODE_LINKED_MODULE_CONTEXT_AWARE(electron_browser_global_shortcut, Initialize)
