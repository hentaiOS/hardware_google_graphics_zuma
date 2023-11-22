/*
 * Copyright (C) 2022 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef EXYNOS_DISPLAY_DRM_INTERFACE_MODULE_ZUMA_H
#define EXYNOS_DISPLAY_DRM_INTERFACE_MODULE_ZUMA_H

#include <drm/samsung_drm.h>

#include "../../gs201/libhwc2.1/libdisplayinterface/ExynosDisplayDrmInterfaceModule.h"

namespace zuma {

class ExynosPrimaryDisplayDrmInterfaceModule
      : public gs201::ExynosPrimaryDisplayDrmInterfaceModule {
public:
    ExynosPrimaryDisplayDrmInterfaceModule(ExynosDisplay *exynosDisplay);
// TODO: b/295990513 - Remove the if defined after kernel prebuilts are merged.
#if defined(EXYNOS_HISTOGRAM_CHANNEL_REQUEST)
    virtual int32_t sendHistogramChannelIoctl(HistogramChannelIoctl_t control,
                                              uint8_t channelId) const override;
#endif
};

using ExynosExternalDisplayDrmInterfaceModule = gs201::ExynosExternalDisplayDrmInterfaceModule;

} // namespace zuma

#endif // EXYNOS_DISPLAY_DRM_INTERFACE_MODULE_ZUMA_H
