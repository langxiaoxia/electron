// Copyright (c) 2021 xxlang@grandstream.cn.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#ifndef SHELL_BROWSER_HID_HID_CHOOSER_CONTEXT_H_
#define SHELL_BROWSER_HID_HID_CHOOSER_CONTEXT_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/unguessable_token.h"
#include "components/keyed_service/core/keyed_service.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "content/public/browser/hid_delegate.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/mojom/hid.mojom.h"
#include "url/origin.h"

namespace base {
class Value;
}

namespace electron {

class HidChooserContext : public KeyedService,
                          public device::mojom::HidManagerClient {
 public:
  using DeviceObserver = content::HidDelegate::Observer;

  HidChooserContext();
  ~HidChooserContext() override;

  // Returns a human-readable string identifier for |device|.
  static std::u16string DisplayNameFromDeviceInfo(
      const device::mojom::HidDeviceInfo& device);

  // Returns true if a persistent permission can be granted for |device|.
  static bool CanStorePersistentEntry(
      const device::mojom::HidDeviceInfo& device);

  // HID-specific interface for granting and checking permissions.
  void GrantDevicePermission(const url::Origin& requesting_origin,
                             const url::Origin& embedding_origin,
                             const device::mojom::HidDeviceInfo& device);
  bool HasDevicePermission(const url::Origin& requesting_origin,
                           const url::Origin& embedding_origin,
                           const device::mojom::HidDeviceInfo& device);

  // For ScopedObserver.
  void AddDeviceObserver(DeviceObserver* observer);
  void RemoveDeviceObserver(DeviceObserver* observer);

  // Only call this if you're sure |devices_| has been initialized before-hand.
  // The returned raw pointer is owned by |devices_| and will be destroyed when
  // the device is removed.
  const device::mojom::HidDeviceInfo* GetDeviceInfo(const std::string& guid);

  device::mojom::HidManager* GetHidManager();

  base::WeakPtr<HidChooserContext> AsWeakPtr();

 private:
  // device::mojom::HidManagerClient implementation:
  void DeviceAdded(device::mojom::HidDeviceInfoPtr device_info) override;
  void DeviceRemoved(device::mojom::HidDeviceInfoPtr device_info) override;
  void DeviceChanged(device::mojom::HidDeviceInfoPtr device_info) override;

  void EnsureHidManagerConnection();
  void SetUpHidManagerConnection(
      mojo::PendingRemote<device::mojom::HidManager> manager);
  void InitDeviceList(std::vector<device::mojom::HidDeviceInfoPtr> devices);
  void OnHidManagerConnectionError();

  bool is_initialized_ = false;

  // Tracks the set of devices to which an origin (potentially embedded in another
  // origin) has access to. Key is (requesting_origin, embedding_origin).
  std::map<std::pair<url::Origin, url::Origin>,
           std::set<std::string>>
      ephemeral_devices_;

  // Map from device GUID to device info.
  std::map<std::string, device::mojom::HidDeviceInfoPtr> devices_;

  mojo::Remote<device::mojom::HidManager> hid_manager_;
  mojo::AssociatedReceiver<device::mojom::HidManagerClient> client_receiver_{
      this};
  base::ObserverList<DeviceObserver> device_observer_list_;

  base::WeakPtrFactory<HidChooserContext> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(HidChooserContext);
};

}  // namespace electron

#endif  // SHELL_BROWSER_HID_HID_CHOOSER_CONTEXT_H_
