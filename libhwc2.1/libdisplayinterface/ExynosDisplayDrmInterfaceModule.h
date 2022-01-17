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

#include "../../gs101/libhwc2.1/libdisplayinterface/ExynosDisplayDrmInterfaceModule.h"

namespace zuma {

/*
 * TODO: Zuma CGC DMA changes in kernel are yet to be implemented.
 *       Deriving from gs101 until implementation is complete.
 *       Bug: 219110321
 */

using ExynosPrimaryDisplayDrmInterfaceModule = gs101::ExynosPrimaryDisplayDrmInterfaceModule;
using ExynosExternalDisplayDrmInterfaceModule = gs101::ExynosExternalDisplayDrmInterfaceModule;

}  // namespace zuma

#endif // EXYNOS_DISPLAY_DRM_INTERFACE_MODULE_ZUMA_H
