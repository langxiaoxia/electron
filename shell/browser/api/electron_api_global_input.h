// Copyright (c) 2014 GitHub, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#ifndef ELECTRON_SHELL_BROWSER_API_ELECTRON_API_GLOBAL_INPUT_H_
#define ELECTRON_SHELL_BROWSER_API_ELECTRON_API_GLOBAL_INPUT_H_

#include "gin/handle.h"
#include "gin/wrappable.h"

namespace electron::api {

class GlobalInput : public gin::Wrappable<GlobalInput> {
 public:
  static gin::Handle<GlobalInput> Create(v8::Isolate* isolate);

  // gin::Wrappable
  static gin::WrapperInfo kWrapperInfo;
  gin::ObjectTemplateBuilder GetObjectTemplateBuilder(
      v8::Isolate* isolate) override;
  const char* GetTypeName() override;

  // disable copy
  GlobalInput(const GlobalInput&) = delete;
  GlobalInput& operator=(const GlobalInput&) = delete;

 protected:
  explicit GlobalInput(v8::Isolate* isolate);
  ~GlobalInput() override;

 private:
  bool SendMouse(const std::string& id,
                 int action,
                 int buttons,
                 int x,
                 int y,
                 int width,
                 int height,
                 bool alt,
                 bool ctrl,
                 bool shift,
                 bool meta);
  bool SendKeyboard(bool down,
                    int vkey,
                    bool alt,
                    bool ctrl,
                    bool shift,
                    bool meta);
};

}  // namespace electron::api

#endif  // ELECTRON_SHELL_BROWSER_API_ELECTRON_API_GLOBAL_INPUT_H_
