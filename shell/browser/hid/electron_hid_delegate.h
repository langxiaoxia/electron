// Copyright (c) 2021 xxlang@grandstream.cn.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#ifndef SHELL_BROWSER_HID_ELECTRON_HID_DELEGATE_H_
#define SHELL_BROWSER_HID_ELECTRON_HID_DELEGATE_H_

#include <memory>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "content/public/browser/hid_delegate.h"
#include "shell/browser/hid/hid_chooser_controller.h"

namespace electron {

class HidChooserController;

class ElectronHidDelegate : public content::HidDelegate {
 public:
  ElectronHidDelegate();
  ~ElectronHidDelegate() override;

  std::unique_ptr<content::HidChooser> RunChooser(
      content::RenderFrameHost* frame,
      std::vector<blink::mojom::HidDeviceFilterPtr> filters,
      content::HidChooser::Callback callback) override;
  bool CanRequestDevicePermission(content::WebContents* web_contents) override;
  bool HasDevicePermission(content::WebContents* web_contents,
                           const device::mojom::HidDeviceInfo& device) override;
  device::mojom::HidManager* GetHidManager(
      content::WebContents* web_contents) override;
  void AddObserver(content::RenderFrameHost* frame,
                   content::HidDelegate::Observer* observer) override;
  void RemoveObserver(content::RenderFrameHost* frame,
                      content::HidDelegate::Observer* observer) override;
  const device::mojom::HidDeviceInfo* GetDeviceInfo(
      content::WebContents* web_contents,
      const std::string& guid) override;

 private:
  HidChooserController* ControllerForFrame(
      content::RenderFrameHost* render_frame_host);
  HidChooserController* AddControllerForFrame(
      content::RenderFrameHost* render_frame_host,
      std::vector<blink::mojom::HidDeviceFilterPtr> filters,
      content::HidChooser::Callback callback);
  void DeleteControllerForFrame(content::RenderFrameHost* render_frame_host);

  std::unordered_map<content::RenderFrameHost*,
                     std::unique_ptr<HidChooserController>>
      controller_map_;

  base::WeakPtrFactory<ElectronHidDelegate> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ElectronHidDelegate);
};

}  // namespace electron

#endif  // SHELL_BROWSER_HID_ELECTRON_HID_DELEGATE_H_
