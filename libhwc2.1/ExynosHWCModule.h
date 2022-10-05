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

#ifndef ANDROID_EXYNOS_HWC_MODULE_ZUMA_H_
#define ANDROID_EXYNOS_HWC_MODULE_ZUMA_H_

#include "../../gs201/libhwc2.1/ExynosHWCModule.h"

namespace zuma {

static const char *early_wakeup_node_0_base =
    "/sys/devices/platform/19470000.drmdecon/early_wakeup";

static const dpp_channel_map_t idma_channel_map[] = {
    /* GF physical index is switched to change assign order */
    /* DECON_IDMA is not used */
    {MPP_DPP_GFS,     0, IDMA(0),   IDMA(0)},
    {MPP_DPP_VGRFS,   0, IDMA(1),   IDMA(1)},
    {MPP_DPP_GFS,     1, IDMA(2),   IDMA(2)},
    {MPP_DPP_VGRFS,   1, IDMA(3),   IDMA(3)},
    {MPP_DPP_GFS,     2, IDMA(4),   IDMA(4)},
    {MPP_DPP_VGRFS,   2, IDMA(5),   IDMA(5)},
    {MPP_DPP_GFS,     3, IDMA(6),   IDMA(6)},
    {MPP_DPP_GFS,     4, IDMA(7),   IDMA(7)},
    {MPP_DPP_VGRFS,   3, IDMA(8),   IDMA(8)},
    {MPP_DPP_GFS,     5, IDMA(9),   IDMA(9)},
    {MPP_DPP_VGRFS,   4, IDMA(10),  IDMA(10)},
    {MPP_DPP_GFS,     6, IDMA(11),  IDMA(11)},
    {MPP_DPP_VGRFS,   5, IDMA(12),  IDMA(12)},
    {MPP_DPP_GFS,     7, IDMA(13),  IDMA(13)},
    {MPP_P_TYPE_MAX,  0, IDMA(14),  IDMA(14)}, // not idma but..
    {static_cast<mpp_phycal_type_t>(MAX_DECON_DMA_TYPE), 0, MAX_DECON_DMA_TYPE, IDMA(14)}
};

static const exynos_mpp_t available_otf_mpp_units[] = {
    {MPP_DPP_GFS, MPP_LOGICAL_DPP_GFS, "DPP_GFS0", 0, 0, HWC_DISPLAY_PRIMARY_BIT},
    {MPP_DPP_GFS, MPP_LOGICAL_DPP_GFS, "DPP_GFS1", 1, 0, HWC_DISPLAY_PRIMARY_BIT},
    {MPP_DPP_GFS, MPP_LOGICAL_DPP_GFS, "DPP_GFS2", 2, 0, HWC_DISPLAY_SECONDARY_BIT},
    {MPP_DPP_GFS, MPP_LOGICAL_DPP_GFS, "DPP_GFS3", 3, 0, HWC_DISPLAY_SECONDARY_BIT},
    {MPP_DPP_GFS, MPP_LOGICAL_DPP_GFS, "DPP_GFS4", 4, 0, HWC_DISPLAY_SECONDARY_BIT},
    {MPP_DPP_GFS, MPP_LOGICAL_DPP_GFS, "DPP_GFS5", 5, 0, HWC_DISPLAY_SECONDARY_BIT},
    {MPP_DPP_GFS, MPP_LOGICAL_DPP_GFS, "DPP_GFS6", 6, 0, HWC_DISPLAY_SECONDARY_BIT},
    {MPP_DPP_GFS, MPP_LOGICAL_DPP_GFS, "DPP_GFS7", 7, 0, HWC_DISPLAY_SECONDARY_BIT},
    {MPP_DPP_VGRFS, MPP_LOGICAL_DPP_VGRFS, "DPP_VGRFS0", 0, 0, HWC_DISPLAY_PRIMARY_BIT},
    {MPP_DPP_VGRFS, MPP_LOGICAL_DPP_VGRFS, "DPP_VGRFS1", 1, 0, HWC_DISPLAY_PRIMARY_BIT},
    {MPP_DPP_VGRFS, MPP_LOGICAL_DPP_VGRFS, "DPP_VGRFS2", 2, 0, HWC_DISPLAY_PRIMARY_BIT},
    {MPP_DPP_VGRFS, MPP_LOGICAL_DPP_VGRFS, "DPP_VGRFS3", 3, 0, HWC_DISPLAY_PRIMARY_BIT},
    {MPP_DPP_VGRFS, MPP_LOGICAL_DPP_VGRFS, "DPP_VGRFS4", 4, 0, HWC_DISPLAY_PRIMARY_BIT},
    {MPP_DPP_VGRFS, MPP_LOGICAL_DPP_VGRFS, "DPP_VGRFS5", 5, 0, HWC_DISPLAY_PRIMARY_BIT},

};

} // namespace zuma

#endif // ANDROID_EXYNOS_HWC_MODULE_ZUMA_H_
