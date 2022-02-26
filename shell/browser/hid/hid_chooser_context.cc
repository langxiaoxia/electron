// Copyright (c) 2021 xxlang@grandstream.cn.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#include "shell/browser/hid/hid_chooser_context.h"

#include <utility>

#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "content/public/browser/device_service.h"
#include "services/device/public/cpp/hid/hid_blocklist.h"
#include "ui/base/l10n/l10n_util.h"
#include "base/logging.h"

namespace electron {

constexpr char kHidDeviceNameKey[] = "name";
constexpr char kHidGuidKey[] = "guid";
constexpr char kHidVendorIdKey[] = "vendor-id";
constexpr char kHidProductIdKey[] = "product-id";
constexpr char kHidSerialNumberKey[] = "serial-number";

base::Value DeviceInfoToValue(const device::mojom::HidDeviceInfo& device) {
  base::Value value(base::Value::Type::DICTIONARY);
  value.SetStringKey(
      kHidDeviceNameKey,
      base::UTF16ToUTF8(HidChooserContext::DisplayNameFromDeviceInfo(device)));
  value.SetIntKey(kHidVendorIdKey, device.vendor_id);
  value.SetIntKey(kHidProductIdKey, device.product_id);
  if (HidChooserContext::CanStorePersistentEntry(device)) {
    // Use the USB serial number as a persistent identifier. If it is
    // unavailable, only ephemeral permissions may be granted.
    value.SetStringKey(kHidSerialNumberKey, device.serial_number);
  } else {
    // The GUID is a temporary ID created on connection that remains valid until
    // the device is disconnected. Ephemeral permissions are keyed by this ID
    // and must be granted again each time the device is connected.
    value.SetStringKey(kHidGuidKey, device.guid);
  }
  return value;
}

HidChooserContext::HidChooserContext() = default;
HidChooserContext::~HidChooserContext() = default;

// static
std::u16string HidChooserContext::DisplayNameFromDeviceInfo(
    const device::mojom::HidDeviceInfo& device) {
  if (device.product_name.empty()) {
    auto device_id_string = base::ASCIIToUTF16(
        base::StringPrintf("%04X:%04X", device.vendor_id, device.product_id));
    return device_id_string;
//    return l10n_util::GetStringFUTF16(IDS_HID_CHOOSER_ITEM_WITHOUT_NAME, device_id_string);
  }
  return base::UTF8ToUTF16(device.product_name);
}

// static
bool HidChooserContext::CanStorePersistentEntry(
    const device::mojom::HidDeviceInfo& device) {
  return !device.serial_number.empty() && !device.product_name.empty();
}

void HidChooserContext::GrantDevicePermission(
    const url::Origin& requesting_origin,
    const url::Origin& embedding_origin,
    const device::mojom::HidDeviceInfo& device) {
  DCHECK(base::Contains(devices_, device.guid));
  ephemeral_devices_[{requesting_origin, embedding_origin}].insert(device.guid);
}

bool HidChooserContext::HasDevicePermission(
    const url::Origin& requesting_origin,
    const url::Origin& embedding_origin,
    const device::mojom::HidDeviceInfo& device) {
  auto it = ephemeral_devices_.find({requesting_origin, embedding_origin});
  if (it != ephemeral_devices_.end() &&
      base::Contains(it->second, device.guid)) {
    return true;
  }

  return false;
}

void HidChooserContext::AddDeviceObserver(DeviceObserver* observer) {
  EnsureHidManagerConnection();
  device_observer_list_.AddObserver(observer);
}

void HidChooserContext::RemoveDeviceObserver(DeviceObserver* observer) {
  device_observer_list_.RemoveObserver(observer);
}

const device::mojom::HidDeviceInfo* HidChooserContext::GetDeviceInfo(
    const std::string& guid) {
  DCHECK(is_initialized_);
  auto it = devices_.find(guid);
  return it == devices_.end() ? nullptr : it->second.get();
}

device::mojom::HidManager* HidChooserContext::GetHidManager() {
  EnsureHidManagerConnection();
  return hid_manager_.get();
}

base::WeakPtr<HidChooserContext> HidChooserContext::AsWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void HidChooserContext::DeviceAdded(device::mojom::HidDeviceInfoPtr device) {
  DCHECK(device);

  // Update the device list.
  if (!base::Contains(devices_, device->guid))
    devices_.insert({device->guid, device->Clone()});

  // Notify all observers.
  for (auto& observer : device_observer_list_)
    observer.OnDeviceAdded(*device);
}

void HidChooserContext::DeviceRemoved(device::mojom::HidDeviceInfoPtr device) {
  DCHECK(device);
  DCHECK(base::Contains(devices_, device->guid));

  // Update the device list.
  devices_.erase(device->guid);

  // Notify all device observers.
  for (auto& observer : device_observer_list_)
    observer.OnDeviceRemoved(*device);

  std::vector<std::pair<url::Origin, url::Origin>> revoked_url_pairs;
  for (auto& map_entry : ephemeral_devices_) {
    if (map_entry.second.erase(device->guid) > 0) {
      revoked_url_pairs.push_back(map_entry.first);
    }
  }
}

void HidChooserContext::DeviceChanged(device::mojom::HidDeviceInfoPtr device) {
  DCHECK(device);
  DCHECK(base::Contains(devices_, device->guid));

  // Update the device list.
  devices_[device->guid] = device->Clone();

  // Notify all observers.
  for (auto& observer : device_observer_list_)
    observer.OnDeviceChanged(*device);
}

void HidChooserContext::EnsureHidManagerConnection() {
  if (hid_manager_)
    return;

  mojo::PendingRemote<device::mojom::HidManager> manager;
  content::GetDeviceService().BindHidManager(
      manager.InitWithNewPipeAndPassReceiver());
  SetUpHidManagerConnection(std::move(manager));
}

void HidChooserContext::SetUpHidManagerConnection(
    mojo::PendingRemote<device::mojom::HidManager> manager) {
  hid_manager_.Bind(std::move(manager));
  hid_manager_.set_disconnect_handler(base::BindOnce(
      &HidChooserContext::OnHidManagerConnectionError, base::Unretained(this)));

  hid_manager_->GetDevicesAndSetClient(
      client_receiver_.BindNewEndpointAndPassRemote(),
      base::BindOnce(&HidChooserContext::InitDeviceList,
                     weak_factory_.GetWeakPtr()));
}

void HidChooserContext::InitDeviceList(
    std::vector<device::mojom::HidDeviceInfoPtr> devices) {
  for (auto& device : devices)
    devices_.insert({device->guid, std::move(device)});

  is_initialized_ = true;
}

void HidChooserContext::OnHidManagerConnectionError() {
  hid_manager_.reset();
  client_receiver_.reset();
  devices_.clear();

  ephemeral_devices_.clear();
}

}  // namespace electron
