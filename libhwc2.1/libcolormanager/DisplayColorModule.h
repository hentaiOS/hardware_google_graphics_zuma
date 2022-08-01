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

#pragma once

#include <zuma/displaycolor/displaycolor_zuma.h>

#include "DisplayColorLoader.h"
#include "drmdevice.h"

/*
 * TODO: Temporary fix until kernel dependencies for zuma are not fixed
 *       Bug: 225778788
 */

#ifndef DRM_SAMSUNG_CGC_DMA_LUT_ENTRY_CNT
#define DRM_SAMSUNG_CGC_DMA_LUT_ENTRY_CNT 4913
/**
 * struct cgc_dma_lut - color gammut control format for cgc dma to set by user-space
 *
 * @r_value: value for red color
 * @g_value: value for green color
 * @b_value: value for blue color
 *
 * A cgc_dma_lut represents a format to support cgc dma. cgc coefficients should be
 * located in dram according to this format.
 */
struct cgc_dma_lut {
	__u16 r_value;
	__u16 g_value;
	__u16 b_value;
};
#endif

namespace gs {

static constexpr char kGsEntry[] = "GetDisplayColorZuma";

class ColorDrmBlobFactory {
public:
    using GsInterfaceType = displaycolor::IDisplayColorZuma;
    using DcLoaderType = DisplayColorLoader<GsInterfaceType, kGsEntry>;

    static int32_t eotf(const GsInterfaceType::IDpp::EotfData::ConfigType *config,
                        android::DrmDevice *drm, uint32_t &blobId);
    static int32_t gm(const GsInterfaceType::IDpp::GmData::ConfigType *config,
                      android::DrmDevice *drm, uint32_t &blobId);
    static int32_t dtm(const GsInterfaceType::IDpp::DtmData::ConfigType *config,
                       android::DrmDevice *drm, uint32_t &blobId);
    static int32_t oetf(const GsInterfaceType::IDpp::OetfData::ConfigType *config,
                        android::DrmDevice *drm, uint32_t &blobId);
    static int32_t gammaMatrix(const GsInterfaceType::IDqe::DqeMatrixData::ConfigType *config,
                               android::DrmDevice *drm, uint32_t &blobId);
    static int32_t degamma(const uint64_t drmLutSize,
                           const GsInterfaceType::IDqe::DegammaLutData::ConfigType *config,
                           android::DrmDevice *drm, uint32_t &blobId);
    static int32_t linearMatrix(const GsInterfaceType::IDqe::DqeMatrixData::ConfigType *config,
                                android::DrmDevice *drm, uint32_t &blobId);
    static int32_t cgc(const GsInterfaceType::IDqe::CgcData::ConfigType *config,
                       android::DrmDevice *drm, uint32_t &blobId);
    static int32_t cgcDither(const GsInterfaceType::IDqe::DqeControlData::ConfigType *config,
                             android::DrmDevice *drm, uint32_t &blobId);
    static int32_t regamma(const uint64_t drmLutSize,
                           const GsInterfaceType::IDqe::RegammaLutData::ConfigType *config,
                           android::DrmDevice *drm, uint32_t &blobId);
    static int32_t displayDither(const GsInterfaceType::IDqe::DqeControlData::ConfigType *config,
                                 android::DrmDevice *drm, uint32_t &blobId);
};

} // namespace gs
