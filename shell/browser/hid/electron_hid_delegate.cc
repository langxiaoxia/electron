// Copyright (c) 2021 xxlang@grandstream.cn.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#include "shell/browser/hid/electron_hid_delegate.h"

#include <utility>

#include "base/feature_list.h"
#include "content/public/browser/web_contents.h"
#include "shell/browser/api/electron_api_web_contents.h"
#include "shell/browser/hid/hid_chooser_context.h"
#include "shell/browser/hid/hid_chooser_context_factory.h"

using content::RenderFrameHost;
using content::WebContents;

namespace features {

const base::Feature kElectronHidChooser{ "ElectronHidChooser",
                                        base::FEATURE_ENABLED_BY_DEFAULT };
}

namespace electron {

HidChooserContext* GetChooserContext(content::WebContents* web_contents) {
  auto* browser_context = web_contents->GetBrowserContext();
  return HidChooserContextFactory::GetForBrowserContext(browser_context);
}

ElectronHidDelegate::ElectronHidDelegate() = default;

ElectronHidDelegate::~ElectronHidDelegate() = default;

std::unique_ptr<content::HidChooser>
ElectronHidDelegate::RunChooser(
    content::RenderFrameHost* frame,
    std::vector<blink::mojom::HidDeviceFilterPtr> filters,
    content::HidChooser::Callback callback) {
  if (base::FeatureList::IsEnabled(features::kElectronHidChooser)) {
    HidChooserController* controller = ControllerForFrame(frame);
    if (controller) {
      DeleteControllerForFrame(frame);
    }
    AddControllerForFrame(frame, std::move(filters), std::move(callback));
  } else {
    // If feature is disabled, immediately return back with no device selected.
    std::move(callback).Run({});
  }

  // Return a nullptr because the return value isn't used for anything, eg
  // there is no mechanism to cancel navigator.hid.requestDevice(). The return
  // value is simply used in Chromium to cleanup the chooser UI once the hid
  // service is destroyed.
  return nullptr;
}

// The following methods are not currently called in Electron.
bool ElectronHidDelegate::CanRequestDevicePermission(
    content::WebContents* web_contents) {
  NOTIMPLEMENTED();
  return true;
}

bool ElectronHidDelegate::HasDevicePermission(
    content::WebContents* web_contents,
    const device::mojom::HidDeviceInfo& device) {
  NOTIMPLEMENTED();
  return true;
}

device::mojom::HidManager* ElectronHidDelegate::GetHidManager(
    content::WebContents* web_contents) {
  return GetChooserContext(web_contents)->GetHidManager();
}

void ElectronHidDelegate::AddObserver(content::RenderFrameHost* frame,
                                    Observer* observer) {
  NOTIMPLEMENTED();
}

void ElectronHidDelegate::RemoveObserver(
    content::RenderFrameHost* frame,
    content::HidDelegate::Observer* observer) {
  NOTIMPLEMENTED();
}

const device::mojom::HidDeviceInfo* ElectronHidDelegate::GetDeviceInfo(
    content::WebContents* web_contents,
    const std::string& guid) {
  return GetChooserContext(web_contents)->GetDeviceInfo(guid);
}

HidChooserController* ElectronHidDelegate::ControllerForFrame(
    content::RenderFrameHost* render_frame_host) {
  auto mapping = controller_map_.find(render_frame_host);
  return mapping == controller_map_.end() ? nullptr : mapping->second.get();
}

HidChooserController* ElectronHidDelegate::AddControllerForFrame(
    content::RenderFrameHost* render_frame_host,
    std::vector<blink::mojom::HidDeviceFilterPtr> filters,
    content::HidChooser::Callback callback) {
  auto* web_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host);
  auto controller = std::make_unique<HidChooserController>(
      render_frame_host, std::move(filters), std::move(callback), web_contents,
      weak_factory_.GetWeakPtr());
  controller_map_.insert(
      std::make_pair(render_frame_host, std::move(controller)));
  return ControllerForFrame(render_frame_host);
}

void ElectronHidDelegate::DeleteControllerForFrame(
    content::RenderFrameHost* render_frame_host) {
  controller_map_.erase(render_frame_host);
}

} // namespace electron
