// Copyright (c) 2021 xxlang@grandstream.cn.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#include "shell/browser/hid/hid_chooser_controller.h"

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "shell/browser/api/electron_api_session.h"
#include "shell/browser/hid/hid_chooser_context.h"
#include "shell/browser/hid/hid_chooser_context_factory.h"
#include "shell/common/gin_converters/callback_converter.h"
#include "shell/common/gin_converters/content_converter.h"
#include "shell/common/gin_helper/dictionary.h"
#include "ui/base/l10n/l10n_util.h"
#include "base/logging.h"

namespace gin {

template <>
struct Converter<device::mojom::HidDeviceInfoPtr> {
  static v8::Local<v8::Value> ToV8(
      v8::Isolate* isolate,
      const device::mojom::HidDeviceInfoPtr& device) {
    gin_helper::Dictionary dict = gin::Dictionary::CreateEmpty(isolate);
    dict.Set("vendorId", base::StringPrintf("%u", device->vendor_id));
    dict.Set("productId", base::StringPrintf("%u", device->product_id));
    dict.Set("productName", device->product_name);
    dict.Set("deviceId", device->physical_device_id);
    dict.Set("serialNumber", device->serial_number);
    dict.Set("containerId", device->physical_device_id);
    return gin::ConvertToV8(isolate, dict);
  }
};

}  // namespace gin

namespace {

std::string PhysicalDeviceIdFromDeviceInfo(
    const device::mojom::HidDeviceInfo& device) {
  // A single physical device may expose multiple HID interfaces, each
  // represented by a HidDeviceInfo object. When a device exposes multiple
  // HID interfaces, the HidDeviceInfo objects will share a common
  // |physical_device_id|. Group these devices so that a single chooser item
  // is shown for each physical device. If a device's physical device ID is
  // empty, use its GUID instead.
  return device.physical_device_id.empty() ? device.guid
                                           : device.physical_device_id;
}

}  // namespace

namespace electron {

HidChooserController::HidChooserController(
    content::RenderFrameHost* render_frame_host,
    std::vector<blink::mojom::HidDeviceFilterPtr> filters,
    content::HidChooser::Callback callback,
    content::WebContents* web_contents,
    base::WeakPtr<ElectronHidDelegate> hid_delegate)
    : WebContentsObserver(web_contents),
      filters_(std::move(filters)),
      callback_(std::move(callback)),
      hid_delegate_(hid_delegate) {
  requesting_origin_ = render_frame_host->GetLastCommittedOrigin();
  embedding_origin_ = web_contents->GetMainFrame()->GetLastCommittedOrigin();

  chooser_context_ = HidChooserContextFactory::GetForBrowserContext(
                         web_contents->GetBrowserContext())
                         ->AsWeakPtr();
  DCHECK(chooser_context_);
  chooser_context_->GetHidManager()->GetDevices(base::BindOnce(
      &HidChooserController::OnGotDevices, weak_factory_.GetWeakPtr()));
}

HidChooserController::~HidChooserController() {
  if (callback_)
    std::move(callback_).Run({});
  if (chooser_context_) {
    chooser_context_->RemoveDeviceObserver(this);
  }
}

api::Session* HidChooserController::GetSession() {
  if (!web_contents()) {
    return nullptr;
  }
  return api::Session::FromBrowserContext(web_contents()->GetBrowserContext());
}

void HidChooserController::OnDeviceAdded(
    const device::mojom::HidDeviceInfo& device) {
  api::Session* session = GetSession();
  if (AddDeviceInfo(device) && session) {
    session->Emit("hid-device-added", device.Clone(), web_contents());
  }
}

void HidChooserController::OnDeviceRemoved(
    const device::mojom::HidDeviceInfo& device) {
  api::Session* session = GetSession();
  if (RemoveDeviceInfo(device) && session) {
    session->Emit("hid-device-removed", device.Clone(), web_contents());
  }
}

void HidChooserController::OnDeviceChanged(
    const device::mojom::HidDeviceInfo& device) {
  bool has_chooser_item =
      base::Contains(items_, PhysicalDeviceIdFromDeviceInfo(device));
  if (!has_chooser_item) {
    OnDeviceAdded(device);
    return;
  }

  // Update the item to replace the old device info with |device|.
  UpdateDeviceInfo(device);
}

void HidChooserController::OnDeviceChosen(const std::string& device_id) {
  if (device_id.empty()) {
    std::move(callback_).Run({});
  } else {
    const auto find_it = device_map_.find(device_id);
    if (find_it != device_map_.end()) {
      std::vector<device::mojom::HidDeviceInfoPtr> device_infos;
      for (auto& device : find_it->second) {
        chooser_context_->GrantDevicePermission(requesting_origin_,
          embedding_origin_, *device);
        device_infos.push_back(device->Clone());
      }
      std::move(callback_).Run(std::move(device_infos));
    } else {
      std::move(callback_).Run({});
    }
  }
}

void HidChooserController::OnGotDevices(
    std::vector<device::mojom::HidDeviceInfoPtr> devices) {
  std::vector<device::mojom::HidDeviceInfoPtr> device_infos;
  for (auto& device : devices) {
    if (FilterMatchesAny(*device)) {
      AddDeviceInfo(*device);
      device_infos.push_back(device->Clone());
    }
  }

  bool prevent_default = false;
  api::Session* session = GetSession();
  if (session) {
    prevent_default =
        session->Emit("select-hid-device", device_infos, web_contents(),
                      base::AdaptCallbackForRepeating(base::BindOnce(
                          &HidChooserController::OnDeviceChosen,
                          weak_factory_.GetWeakPtr())));
  }
  if (!prevent_default) {
    std::move(callback_).Run(std::move(device_infos));
  }
}

bool HidChooserController::FilterMatchesAny(
    const device::mojom::HidDeviceInfo& device) const {
  if (filters_.empty())
    return true;

  for (const auto& filter : filters_) {
    if (filter->device_ids) {
      if (filter->device_ids->is_vendor()) {
        if (filter->device_ids->get_vendor() != device.vendor_id)
          continue;
      } else if (filter->device_ids->is_vendor_and_product()) {
        const auto& vendor_and_product =
            filter->device_ids->get_vendor_and_product();
        if (vendor_and_product->vendor != device.vendor_id)
          continue;
        if (vendor_and_product->product != device.product_id)
          continue;
      }
    }

    if (filter->usage) {
      if (filter->usage->is_page()) {
        const uint16_t usage_page = filter->usage->get_page();
        auto find_it =
            std::find_if(device.collections.begin(), device.collections.end(),
                         [=](const device::mojom::HidCollectionInfoPtr& c) {
                           return usage_page == c->usage->usage_page;
                         });
        if (find_it == device.collections.end())
          continue;
      } else if (filter->usage->is_usage_and_page()) {
        const auto& usage_and_page = filter->usage->get_usage_and_page();
        auto find_it = std::find_if(
            device.collections.begin(), device.collections.end(),
            [&usage_and_page](const device::mojom::HidCollectionInfoPtr& c) {
              return usage_and_page->usage_page == c->usage->usage_page &&
                     usage_and_page->usage == c->usage->usage;
            });
        if (find_it == device.collections.end())
          continue;
      }
    }

    return true;
  }

  return false;
}

bool HidChooserController::AddDeviceInfo(
    const device::mojom::HidDeviceInfo& device) {
  auto id = PhysicalDeviceIdFromDeviceInfo(device);
  auto find_it = device_map_.find(id);
  if (find_it != device_map_.end()) {
    DVLOG(0) << "HidChooserController::AddDeviceInfo exist id=" << id;
    DVLOG(0) << "guid=" << device.guid;
    DVLOG(0) << "physical_device_id=" << device.physical_device_id;
    DVLOG(0) << "vendor_id=" << device.vendor_id;
    DVLOG(0) << "product_id=" << device.product_id;
    DVLOG(0) << "product_name=" << device.product_name;
    find_it->second.push_back(device.Clone());
    return false;
  }
  // A new device was connected. Append it to the end of the chooser list.
  DVLOG(0) << "HidChooserController::AddDeviceInfo new id=" << id;
  DVLOG(0) << "guid=" << device.guid;
  DVLOG(0) << "physical_device_id=" << device.physical_device_id;
  DVLOG(0) << "vendor_id=" << device.vendor_id;
  DVLOG(0) << "product_id=" << device.product_id;
  DVLOG(0) << "product_name=" << device.product_name;
  device_map_[id].push_back(device.Clone());
  items_.push_back(id);
  return true;
}

bool HidChooserController::RemoveDeviceInfo(
    const device::mojom::HidDeviceInfo& device) {
  auto id = PhysicalDeviceIdFromDeviceInfo(device);
  auto find_it = device_map_.find(id);
  DCHECK(find_it != device_map_.end());
  auto& device_infos = find_it->second;
  base::EraseIf(device_infos,
                [&device](const device::mojom::HidDeviceInfoPtr& d) {
                  return d->guid == device.guid;
                });
  if (!device_infos.empty())
    return false;
  // A device was disconnected. Remove it from the chooser list.
  device_map_.erase(find_it);
  base::Erase(items_, id);
  return true;
}

void HidChooserController::UpdateDeviceInfo(
    const device::mojom::HidDeviceInfo& device) {
  auto id = PhysicalDeviceIdFromDeviceInfo(device);
  auto physical_device_it = device_map_.find(id);
  DCHECK(physical_device_it != device_map_.end());
  auto& device_infos = physical_device_it->second;
  auto device_it = base::ranges::find_if(
      device_infos, [&device](const device::mojom::HidDeviceInfoPtr& d) {
        return d->guid == device.guid;
      });
  DCHECK(device_it != device_infos.end());
  *device_it = device.Clone();
}

}  // namespace electron
