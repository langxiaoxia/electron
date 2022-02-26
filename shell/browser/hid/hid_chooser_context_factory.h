// Copyright (c) 2021 xxlang@grandstream.cn.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#ifndef SHELL_BROWSER_HID_HID_CHOOSER_CONTEXT_FACTORY_H_
#define SHELL_BROWSER_HID_HID_CHOOSER_CONTEXT_FACTORY_H_

#include "base/macros.h"
#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "shell/browser/hid/hid_chooser_context.h"

namespace electron {

class HidChooserContext;

class HidChooserContextFactory : public BrowserContextKeyedServiceFactory {
 public:
  static HidChooserContext* GetForBrowserContext(
      content::BrowserContext* context);
  static HidChooserContextFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<HidChooserContextFactory>;

  HidChooserContextFactory();
  ~HidChooserContextFactory() override;

  // BrowserContextKeyedServiceFactory methods:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;

  DISALLOW_COPY_AND_ASSIGN(HidChooserContextFactory);
};

}  // namespace electron

#endif  // SHELL_BROWSER_HID_HID_CHOOSER_CONTEXT_FACTORY_H_
