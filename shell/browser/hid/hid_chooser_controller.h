// Copyright (c) 2021 xxlang@grandstream.cn.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#ifndef SHELL_BROWSER_HID_HID_CHOOSER_CONTROLLER_H_
#define SHELL_BROWSER_HID_HID_CHOOSER_CONTROLLER_H_

#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/hid_chooser.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "services/device/public/mojom/hid.mojom-forward.h"
#include "shell/browser/api/electron_api_session.h"
#include "shell/browser/hid/electron_hid_delegate.h"
#include "shell/browser/hid/hid_chooser_context.h"
#include "third_party/blink/public/mojom/hid/hid.mojom.h"

namespace content {
class RenderFrameHost;
}  // namespace content

namespace electron {

class ElectronHidDelegate;

// HidChooserController provides data for the WebHID API permission prompt.
class HidChooserController final : public HidChooserContext::DeviceObserver,
                                      public content::WebContentsObserver {
 public:
  HidChooserController(
      content::RenderFrameHost* render_frame_host,
      std::vector<blink::mojom::HidDeviceFilterPtr> filters,
      content::HidChooser::Callback callback,
      content::WebContents* web_contents,
      base::WeakPtr<ElectronHidDelegate> hid_delegate);
  ~HidChooserController() override;

  // HidChooserContext::DeviceObserver:
  void OnDeviceAdded(const device::mojom::HidDeviceInfo& device_info) override;
  void OnDeviceRemoved(
      const device::mojom::HidDeviceInfo& device_info) override;
  void OnDeviceChanged(
      const device::mojom::HidDeviceInfo& device_info) override;
  void OnHidManagerConnectionError() override {}
  void OnPermissionRevoked(const url::Origin& origin) override {}

 private:
  api::Session* GetSession();
  void OnGotDevices(std::vector<device::mojom::HidDeviceInfoPtr> devices);
  bool FilterMatchesAny(const device::mojom::HidDeviceInfo& device) const;

  // Add |device_info| to |device_map_|. The device is added to the chooser item
  // representing the physical device. If the chooser item does not yet exist, a
  // new item is appended. Returns true if an item was appended.
  bool AddDeviceInfo(const device::mojom::HidDeviceInfo& device_info);

  // Remove |device_info| from |device_map_|. The device info is removed from
  // the chooser item representing the physical device. If this would cause the
  // item to be empty, the chooser item is removed. Does nothing if the device
  // is not in the chooser item. Returns true if an item was removed.
  bool RemoveDeviceInfo(const device::mojom::HidDeviceInfo& device_info);

  // Update the information for the device described by |device_info| in the
  // |device_map_|.
  void UpdateDeviceInfo(const device::mojom::HidDeviceInfo& device_info);

  void OnDeviceChosen(const std::string& device_id);

  std::vector<blink::mojom::HidDeviceFilterPtr> filters_;
  content::HidChooser::Callback callback_;
  url::Origin requesting_origin_;
  url::Origin embedding_origin_;

  base::WeakPtr<HidChooserContext> chooser_context_;

  // Information about connected devices and their HID interfaces. A single
  // physical device may expose multiple HID interfaces. Keys are physical
  // device IDs, values are collections of HidDeviceInfo objects representing
  // the HID interfaces hosted by the physical device.
  std::map<std::string, std::vector<device::mojom::HidDeviceInfoPtr>>
      device_map_;

  // An ordered list of physical device IDs that determines the order of items
  // in the chooser.
  std::vector<std::string> items_;

  base::WeakPtr<ElectronHidDelegate> hid_delegate_;

  base::WeakPtrFactory<HidChooserController> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(HidChooserController);
};

}  // namespace electron

#endif  // SHELL_BROWSER_HID_HID_CHOOSER_CONTROLLER_H_
