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

#include "ExynosResourceManagerModule.h"

#include <list>

#include "ExynosLayer.h"

using namespace zuma;

constexpr uint32_t TDM_OVERLAP_MARGIN = 68;

constexpr uint32_t kSramSBWCWidthAlign = 32;
constexpr uint32_t kSramSBWCWidthMargin = kSramSBWCWidthAlign - 1;
constexpr uint32_t kSramSBWCRotWidthAlign = 4;
constexpr uint32_t kSramAFBC8B4BAlign = 8;
constexpr uint32_t kSramAFBC8B4BMargin = kSramAFBC8B4BAlign - 1;
constexpr uint32_t kSramAFBC2BAlign = 16;
constexpr uint32_t kSramAFBC2BMargin = kSramAFBC2BAlign - 1;

ExynosResourceManagerModule::ExynosResourceManagerModule(ExynosDevice *device)
: gs201::ExynosResourceManagerModule(device)
{
    // HW Resource Table for TDM based allocation
    mHWResourceTables = &HWResourceTables;
    // TODO (b/266048745): Revert once G2D HDR code for zuma is merged
    mM2mMPPs.clear();
}

ExynosResourceManagerModule::~ExynosResourceManagerModule() {}

bool ExynosResourceManagerModule::checkTDMResource(ExynosDisplay *display, ExynosMPP *currentMPP,
                                                   ExynosMPPSource *mppSrc) {
    std::map<tdm_attr_t, uint32_t> accumulatedAmount;
    uint32_t currentBlockId = currentMPP->getHWBlockId();
    HDEBUGLOGD(eDebugTDM, "%s : %p trying to assign to %s, compare with layers", __func__,
               mppSrc->mSrcImg.bufferHandle, currentMPP->mName.string());
    for (auto layer : display->mLayers) {
        ExynosMPP *otfMPP = layer->mOtfMPP;
        if (!otfMPP) continue;
        getAmounts(display, otfMPP, currentBlockId, layer, mppSrc, accumulatedAmount);
    }

    if (display->mExynosCompositionInfo.mHasCompositionLayer) {
        HDEBUGLOGD(eDebugTDM,
                   "%s : %p trying to assign to %s, compare with ExynosComposition Target buffer",
                   __func__, mppSrc->mSrcImg.bufferHandle, currentMPP->mName.string());
        ExynosMPP *otfMPP = display->mExynosCompositionInfo.mOtfMPP;
        if (otfMPP)
            getAmounts(display, otfMPP, currentBlockId, &display->mExynosCompositionInfo, mppSrc,
                       accumulatedAmount);
    }

    if (display->mClientCompositionInfo.mHasCompositionLayer) {
        HDEBUGLOGD(eDebugTDM,
                   "%s : %p trying to assign to %s, compare with ClientComposition Target buffer",
                   __func__, mppSrc->mSrcImg.bufferHandle, currentMPP->mName.string());
        ExynosMPP *otfMPP = display->mClientCompositionInfo.mOtfMPP;
        if (otfMPP)
            getAmounts(display, otfMPP, currentBlockId, &display->mClientCompositionInfo, mppSrc,
                       accumulatedAmount);
    }

    DisplayTDMInfo::ResourceAmount_t amount = {0};
    for (auto attr = HWAttrs.begin(); attr != HWAttrs.end(); attr++) {
        uint32_t currentAmount = mppSrc->getHWResourceAmount(attr->first);
        amount = display->mDisplayTDMInfo[currentBlockId].getAvailableAmount(attr->first);
        HDEBUGLOGD(eDebugTDM,
                   "%s, layer[%p] -> %s attr[%s], accumulated : %d, current : %d, total : %d",
                   __func__, mppSrc->mSrcImg.bufferHandle, currentMPP->mName.string(),
                   attr->second.string(), accumulatedAmount[attr->first], currentAmount,
                   amount.totalAmount);
        if (accumulatedAmount[attr->first] + currentAmount > amount.totalAmount) {
            HDEBUGLOGD(eDebugTDM, "%s, %s could not assigned by attr[%s]", __func__,
                       currentMPP->mName.string(), attr->second.string());
            return false;
        }
    }

    HDEBUGLOGD(eDebugTDM, "%s : %p trying to assign to %s successfully", __func__,
               mppSrc->mSrcImg.bufferHandle, currentMPP->mName.string());
    return true;
}

bool ExynosResourceManagerModule::isHWResourceAvailable(ExynosDisplay *display,
                                                        ExynosMPP *currentMPP,
                                                        ExynosMPPSource *mppSrc) {
    if (!checkTDMResource(display, currentMPP, mppSrc)) {
        return false;
    }

    std::list<ExynosLayer *> overlappedLayers;
    uint32_t currentBlockId = currentMPP->getHWBlockId();
    for (auto layer : display->mLayers) {
        ExynosMPP *otfMPP = layer->mOtfMPP;
        if (!otfMPP || dynamic_cast<ExynosMPPSource *>(layer) == mppSrc) continue;

        if ((currentBlockId == otfMPP->getHWBlockId()) && isOverlapped(display, mppSrc, layer))
            overlappedLayers.push_back(layer);
    }

    if (overlappedLayers.size()) {
        HDEBUGLOGD(eDebugTDM,
                   "%s : %p trying to assign to %s, check its overlapped layers(%zu) status",
                   __func__, mppSrc->mSrcImg.bufferHandle, currentMPP->mName.string(),
                   overlappedLayers.size());

        for (auto &overlappedLayer : overlappedLayers) {
            HDEBUGLOGD(eDebugTDM, "%s : %p overlapped %p", __func__, mppSrc->mSrcImg.bufferHandle,
                       overlappedLayer->mLayerBuffer);
            if (!checkTDMResource(display, overlappedLayer->mOtfMPP, overlappedLayer)) {
                return false;
            }
        }
    }
    return true;
}

uint32_t ExynosResourceManagerModule::setDisplaysTDMInfo()
{
    ExynosDisplay *addedDisplay = nullptr;

    /*
     * Checking display connections,
     * Assume that WFD and External are not connected at the same time
     * If non-primary display is connected, primary display's HW resource is looted
     */
    for (auto &display : mDisplays) {
        if (display->mType == HWC_DISPLAY_PRIMARY) continue;
        if (display->isEnabled()) {
            addedDisplay = display;
            break;
        }
    }

    /*
     * Update Primary's resource amount. primary = total - loot(other display's HW resource)
     * Other's aready defined at initDisplaysTDMInfo()
     */
    ExynosDisplay *primaryDisplay = getDisplay(getDisplayId(HWC_DISPLAY_PRIMARY, 0));
    for (auto attr = HWAttrs.begin(); attr != HWAttrs.end(); attr++) {
        for (auto blockId = DPUBlocks.begin(); blockId != DPUBlocks.end(); blockId++) {
            if (mHWResourceTables->find(
                        HWResourceIndexes(attr->first, blockId->first, primaryDisplay->mType)) !=
                mHWResourceTables->end()) {
                uint32_t total = mHWResourceTables
                                         ->at(HWResourceIndexes(attr->first, blockId->first,
                                                                primaryDisplay->mType))
                                         .totalAmount;

                if (addedDisplay != nullptr) {
                    total = total -
                            addedDisplay->mDisplayTDMInfo[blockId->first]
                                    .getAvailableAmount(attr->first)
                                    .totalAmount;
                }

                DisplayTDMInfo::ResourceAmount_t amount = {
                        0,
                };
                amount.totalAmount = total;
                primaryDisplay->mDisplayTDMInfo[blockId->first].initTDMInfo(amount, attr->first);
                HDEBUGLOGD(eDebugTDM, "Primary display (block : %d) : %s amount is updated to %d",
                           blockId->first, attr->second.string(), amount.totalAmount);
            }
        }
    }

    if (hwcCheckDebugMessages(eDebugTDM)) {
        for (auto &display : mDisplays) {
            for (auto attr = HWAttrs.begin(); attr != HWAttrs.end(); attr++) {
                for (auto blockId = DPUBlocks.begin(); blockId != DPUBlocks.end(); blockId++) {
                    DisplayTDMInfo::ResourceAmount_t amount = {
                            0,
                    };
                    amount = display->mDisplayTDMInfo[blockId->first].getAvailableAmount(
                            attr->first);
                    HDEBUGLOGD(eDebugTDM, "%s : [%s] display: %d, block : %d, amount : %d(%s)",
                               __func__, attr->second.string(), display->mType, blockId->first,
                               amount.totalAmount, display->isEnabled() ? "used" : "not used");
                }
            }
        }
    }

    return 0;
}

uint32_t ExynosResourceManagerModule::initDisplaysTDMInfo()
{
    /*
     * Initialize as predefined value at table
     * Primary's resource will be changed at setDisplaysTDMInfo() function
     */
    for (auto &display : mDisplays) {
        for (auto attr = HWAttrs.begin(); attr != HWAttrs.end(); attr++) {
            for (auto blockId = DPUBlocks.begin(); blockId != DPUBlocks.end(); blockId++) {
                if (mHWResourceTables->find(
                            HWResourceIndexes(attr->first, blockId->first, display->mType)) !=
                    mHWResourceTables->end()) {
                    DisplayTDMInfo::ResourceAmount_t amount = {
                            0,
                    };
                    amount.totalAmount = mHWResourceTables
                                                 ->at(HWResourceIndexes(attr->first, blockId->first,
                                                                        display->mType))
                                                 .maxAssignedAmount;
                    display->mDisplayTDMInfo[blockId->first].initTDMInfo(amount, attr->first);
                    HDEBUGLOGD(eDebugTDM, "%s, [attr:%d] display : %d, block : %d, amount : %d",
                               __func__, attr->first, display->mType, blockId->first,
                               amount.totalAmount);
                }
            }
        }
    }

    return 0;
}

uint32_t ExynosResourceManagerModule::calculateHWResourceAmount(ExynosDisplay *display,
                                                                ExynosMPPSource *mppSrc)
{
    uint32_t SRAMtotal = 0;

    if (mppSrc == nullptr) return SRAMtotal;

    if (mppSrc->mSourceType == MPP_SOURCE_LAYER) {
        ExynosLayer *layer = static_cast<ExynosLayer *>(mppSrc->mSource);
        if (layer == nullptr) {
            ALOGE("%s: cannot cast ExynosLayer", __func__);
            return SRAMtotal;
        }
        exynos_image src_img;
        exynos_image dst_img;
        layer->setSrcExynosImage(&src_img);
        layer->setDstExynosImage(&dst_img);
        layer->setExynosImage(src_img, dst_img);
    }

    HDEBUGLOGD(eDebugTDM, "mppSrc(%p) SRAM calculation start", mppSrc->mSrcImg.bufferHandle);

    int32_t transform = mppSrc->mSrcImg.transform;
    int32_t compressed = mppSrc->mSrcImg.compressed;
    bool rotation = (transform & HAL_TRANSFORM_ROT_90) ? true : false;

    int32_t width = mppSrc->mSrcImg.w;
    int32_t height = mppSrc->mSrcImg.h;
    uint32_t format = mppSrc->mSrcImg.format;
    uint32_t formatBPP = 0;
    if (isFormat10Bit(format))
        formatBPP = BIT10;
    else if (isFormat8Bit(format))
        formatBPP = BIT8;

    /** To find index **/
    uint32_t formatIndex = 0;

    lbWidthIndex_t widthIndex;

    auto findWidthIndex = [&](int32_t w) -> lbWidthIndex_t {
        for (auto it = LB_WIDTH_INDEX_MAP.begin(); it != LB_WIDTH_INDEX_MAP.end(); it++) {
            if (w >= it->second.widthDownto && w <= it->second.widthUpto) {
                return it->first;
            }
        }
        return LB_W_3073_INF;
    };

    /* Caluclate SRAM amount */
    if (rotation) {
        width = height;
        /* Rotation amount, Only YUV rotation is supported */
        if (isFormatSBWC(format)) {
            /* Y and UV width should be aligned and should get sram for each Y and UV */
            int32_t width_y = pixel_align(width + kSramSBWCRotWidthAlign, kSramSBWCRotWidthAlign);
            int32_t width_c =
                    pixel_align(width / 2 + kSramSBWCRotWidthAlign, kSramSBWCRotWidthAlign);
            widthIndex = findWidthIndex(width_y);
            if (sramAmountMap.find(sramAmountParams(TDM_ATTR_ROT_90, SBWC_Y, widthIndex)) !=
                sramAmountMap.end())
                SRAMtotal +=
                        sramAmountMap.at(sramAmountParams(TDM_ATTR_ROT_90, SBWC_Y, widthIndex));
            widthIndex = findWidthIndex(width_c * 2);
            if (sramAmountMap.find(sramAmountParams(TDM_ATTR_ROT_90, SBWC_UV, widthIndex)) !=
                sramAmountMap.end())
                SRAMtotal +=
                        sramAmountMap.at(sramAmountParams(TDM_ATTR_ROT_90, SBWC_UV, widthIndex));
        } else {
            /* sramAmountMap has SRAM for both Y and UV */
            widthIndex = findWidthIndex(width);
            if (sramAmountMap.find(sramAmountParams(TDM_ATTR_ROT_90, NON_SBWC_Y | formatBPP,
                                                    widthIndex)) != sramAmountMap.end())
                SRAMtotal += sramAmountMap.at(
                        sramAmountParams(TDM_ATTR_ROT_90, NON_SBWC_Y | formatBPP, widthIndex));
            if (sramAmountMap.find(sramAmountParams(TDM_ATTR_ROT_90, NON_SBWC_UV | formatBPP,
                                                    widthIndex)) != sramAmountMap.end())
                SRAMtotal += sramAmountMap.at(
                        sramAmountParams(TDM_ATTR_ROT_90, NON_SBWC_UV | formatBPP, widthIndex));
        }
        HDEBUGLOGD(eDebugTDM, "+ rotation : %d", SRAMtotal);
    } else {
        if (isFormatSBWC(format)) {
            width = pixel_align(width + kSramSBWCWidthMargin, kSramSBWCWidthAlign);
        } else if (compressed) {
            /* Align for 8,4Byte/pixel formats */
            if (formatToBpp(format) > 16) {
                width = pixel_align(width + kSramAFBC8B4BMargin, kSramAFBC8B4BAlign);
            } else {
                /* Align for 2Byte/pixel formats */
                width = pixel_align(width + kSramAFBC2BMargin, kSramAFBC2BAlign);
            }
        }
        widthIndex = findWidthIndex(width);

        /* AFBC amount */
        if (compressed) {
            formatIndex = (isFormatRgb(format) ? RGB : 0) | formatBPP;
            if (sramAmountMap.find(sramAmountParams(TDM_ATTR_AFBC, formatIndex, widthIndex)) !=
                sramAmountMap.end())
                SRAMtotal +=
                        sramAmountMap.at(sramAmountParams(TDM_ATTR_AFBC, formatIndex, widthIndex));
            HDEBUGLOGD(eDebugTDM, "+ AFBC : %d", SRAMtotal);
        }

        /* SBWC amount */
        if (isFormatSBWC(format)) {
            if (sramAmountMap.find(sramAmountParams(TDM_ATTR_SBWC, SBWC_Y, widthIndex)) !=
                sramAmountMap.end())
                SRAMtotal += sramAmountMap.at(sramAmountParams(TDM_ATTR_SBWC, SBWC_Y, widthIndex));
            if (sramAmountMap.find(sramAmountParams(TDM_ATTR_SBWC, SBWC_UV, widthIndex)) !=
                sramAmountMap.end())
                SRAMtotal += sramAmountMap.at(sramAmountParams(TDM_ATTR_SBWC, SBWC_UV, widthIndex));
            HDEBUGLOGD(eDebugTDM, "+ SBWC : %d", SRAMtotal);
        }
    }

    /* ITP (CSC) amount */
    if (isFormatYUV(format)) {
        /** ITP has no size difference, Use width index as LB_W_3073_INF **/
        if (sramAmountMap.find(sramAmountParams(TDM_ATTR_ITP, formatBPP, LB_W_3073_INF)) !=
            sramAmountMap.end())
            SRAMtotal += sramAmountMap.at(sramAmountParams(TDM_ATTR_ITP, formatBPP, LB_W_3073_INF));
        HDEBUGLOGD(eDebugTDM, "+ YUV : %d", SRAMtotal);
    }

    /* Scale amount */
    int srcW = mppSrc->mSrcImg.w;
    int srcH = mppSrc->mSrcImg.h;
    int dstW = mppSrc->mDstImg.w;
    int dstH = mppSrc->mDstImg.h;

    if (!!(transform & HAL_TRANSFORM_ROT_90)) {
        int tmp = dstW;
        dstW = dstH;
        dstH = tmp;
    }

    bool isScaled = ((srcW != dstW) || (srcH != dstH));

    if (isScaled) {
        if (formatHasAlphaChannel(format))
            formatIndex = FORMAT_RGB_MASK;
        else
            formatIndex = FORMAT_YUV_MASK;

        /** Scale has no size difference, Use width index as LB_W_3073_INF **/
        if (sramAmountMap.find(sramAmountParams(TDM_ATTR_SCALE, formatIndex, LB_W_3073_INF)) !=
            sramAmountMap.end())
            SRAMtotal +=
                    sramAmountMap.at(sramAmountParams(TDM_ATTR_SCALE, formatIndex, LB_W_3073_INF));
        HDEBUGLOGD(eDebugTDM, "+ Scale : %d", SRAMtotal);
    }

    for (auto it = HWAttrs.begin(); it != HWAttrs.end(); it++) {
        uint32_t amount = 0;
        if (it->first == TDM_ATTR_SRAM_AMOUNT) {
            amount = SRAMtotal;
        } else {
            amount = needHWResource(display, mppSrc->mSrcImg, mppSrc->mDstImg, it->first);
        }
        mppSrc->setHWResourceAmount(it->first, amount);
    }

    HDEBUGLOGD(eDebugTDM,
               "mppSrc(%p) needed SRAM(%d), SCALE(%d), AFBC(%d), CSC(%d), SBWC(%d), WCG(%d), "
               "ROT(%d)",
               mppSrc->mSrcImg.bufferHandle, SRAMtotal,
               needHWResource(display, mppSrc->mSrcImg, mppSrc->mDstImg, TDM_ATTR_SCALE),
               needHWResource(display, mppSrc->mSrcImg, mppSrc->mDstImg, TDM_ATTR_AFBC),
               needHWResource(display, mppSrc->mSrcImg, mppSrc->mDstImg, TDM_ATTR_ITP),
               needHWResource(display, mppSrc->mSrcImg, mppSrc->mDstImg, TDM_ATTR_SBWC),
               needHWResource(display, mppSrc->mSrcImg, mppSrc->mDstImg, TDM_ATTR_WCG),
               needHWResource(display, mppSrc->mSrcImg, mppSrc->mDstImg, TDM_ATTR_ROT_90));

    return SRAMtotal;
}

int32_t ExynosResourceManagerModule::otfMppReordering(ExynosDisplay *display,
                                                      ExynosMPPVector &otfMPPs,
                                                      struct exynos_image &src,
                                                      struct exynos_image &dst)
{
    int orderingType = isAFBCCompressed(src.bufferHandle)
            ? ORDER_AFBC
            : (needHdrProcessing(display, src, dst) ? ORDER_WCG : ORDER_AXI);

    int usedAFBCCount[DPU_BLOCK_CNT] = {0};
    int usedWCGCount[DPU_BLOCK_CNT] = {0};
    int usedBlockCount[DPU_BLOCK_CNT] = {0};
    int usedAXIPortCount[AXI_PORT_CNT] = {0};

    auto orderPolicy = [&](const void *lhs, const void *rhs) -> bool {
        if (lhs == NULL || rhs == NULL) {
            return 0;
        }

        const ExynosMPPModule *l = (ExynosMPPModule *)lhs;
        const ExynosMPPModule *r = (ExynosMPPModule *)rhs;

        uint32_t assignedStateL = l->mAssignedState & MPP_ASSIGN_STATE_ASSIGNED;
        uint32_t assignedStateR = r->mAssignedState & MPP_ASSIGN_STATE_ASSIGNED;

        if (assignedStateL != assignedStateR) return assignedStateL < assignedStateR;

        if (l->mPhysicalType != r->mPhysicalType) return l->mPhysicalType < r->mPhysicalType;

        if (orderingType == ORDER_AFBC) {
            /* AFBC balancing */
            if ((l->mAttr & MPP_ATTR_AFBC) != (r->mAttr & MPP_ATTR_AFBC))
                return (l->mAttr & MPP_ATTR_AFBC) > (r->mAttr & MPP_ATTR_AFBC);
            if (l->mAttr & MPP_ATTR_AFBC) {
                /* If layer is AFBC, DPU block that AFBC HW block belongs
                 * which has not been used much should be placed in the front */
                if (usedAFBCCount[l->mHWBlockId] != usedAFBCCount[r->mHWBlockId])
                    return usedAFBCCount[l->mHWBlockId] < usedAFBCCount[r->mHWBlockId];
            }
        } else if (orderingType == ORDER_WCG) {
            /* WCG balancing */
            if ((l->mAttr & MPP_ATTR_WCG) != (r->mAttr & MPP_ATTR_WCG))
                return (l->mAttr & MPP_ATTR_WCG) > (r->mAttr & MPP_ATTR_WCG);
            if (l->mAttr & MPP_ATTR_WCG) {
                /* If layer is WCG, DPU block that WCG HW block belongs
                 * which has not been used much should be placed in the front */
                if (usedWCGCount[l->mHWBlockId] != usedWCGCount[r->mHWBlockId])
                    return usedWCGCount[l->mHWBlockId] < usedWCGCount[r->mHWBlockId];
            }
        }

        /* AXI bus balancing */
        /* AXI port which has not been used much should be placed in the front */
        if (usedAXIPortCount[l->mAXIPortId] != usedAXIPortCount[r->mAXIPortId]) {
            return usedAXIPortCount[l->mAXIPortId] < usedAXIPortCount[r->mAXIPortId];
        }
        /* IF MPP connected same AXI port, Block balancing should be regarded after */
        if (usedBlockCount[l->mHWBlockId] != usedBlockCount[r->mHWBlockId])
            return usedBlockCount[l->mHWBlockId] < usedBlockCount[r->mHWBlockId];

        return l->mPhysicalIndex < r->mPhysicalIndex;
    };

    for (auto it : otfMPPs) {
        ExynosMPPModule *mpp = (ExynosMPPModule *)it;
        uint32_t bId = mpp->getHWBlockId();
        uint32_t aId = mpp->getAXIPortId();
        bool isAFBC = false;
        bool isWCG = false;

        if (mpp->mAssignedState & MPP_ASSIGN_STATE_ASSIGNED) {
            ExynosMPPSource *mppSrc = mpp->mAssignedSources[0];
            if ((mppSrc->mSourceType == MPP_SOURCE_LAYER) &&
                (mppSrc->mSrcImg.bufferHandle != nullptr)) {
                if ((mpp->mAttr & MPP_ATTR_AFBC) &&
                    (isAFBCCompressed(mppSrc->mSrcImg.bufferHandle))) {
                    isAFBC = true;
                    usedAFBCCount[bId]++;
                } else if ((mpp->mAttr & MPP_ATTR_WCG) &&
                           (needHdrProcessing(display, mppSrc->mSrcImg, mppSrc->mDstImg))) {
                    isWCG = true;
                    usedWCGCount[bId]++;
                }
            } else if (mppSrc->mSourceType == MPP_SOURCE_COMPOSITION_TARGET) {
                ExynosCompositionInfo *info = (ExynosCompositionInfo *)mppSrc;
                // ESTEVAN_TBD
                // if ((mpp->mAttr & MPP_ATTR_AFBC) && (info->mCompressionInfo.type ==
                // COMP_TYPE_AFBC)) {
                if ((mpp->mAttr & MPP_ATTR_AFBC) &&
                    (isAFBCCompressed(mppSrc->mSrcImg.bufferHandle))) {
                    isAFBC = true;
                    usedAFBCCount[bId]++;
                } else if ((mpp->mAttr & MPP_ATTR_WCG) &&
                           (needHdrProcessing(display, info->mSrcImg, info->mDstImg))) {
                    isWCG = true;
                    usedWCGCount[bId]++;
                }
            }

            HDEBUGLOGD(eDebugLoadBalancing, "%s: %s is assigned (AFBC:%d, WCG:%d), is %s", __func__,
                       mpp->mName.string(), isAFBC, isWCG,
                       (mppSrc->mSourceType == MPP_SOURCE_LAYER) ? "Layer" : "Client Target");
            usedBlockCount[bId]++;
            usedAXIPortCount[aId]++;
        }
    }

    HDEBUGLOGD(eDebugLoadBalancing,
               "Sorting by %s ordering, AFBC(used DPUF0:%d, DPUF1:%d), AXI(used AXI0:%d, AXI1:%d), "
               "BLOCK(used DPUF0:%d, DPUF1:%d)",
               (orderingType == ORDER_AFBC) ? "AFBC" : "_AXI", usedAFBCCount[DPUF0],
               usedAFBCCount[DPUF1], usedAXIPortCount[AXI0], usedAXIPortCount[AXI1],
               usedBlockCount[DPUF0], usedBlockCount[DPUF1]);

    std::sort(otfMPPs.begin(), otfMPPs.end(), orderPolicy);

    String8 after;
    for (uint32_t i = 0; i < otfMPPs.size(); i++) {
        ExynosMPPModule *mpp = (ExynosMPPModule *)otfMPPs[i];
        after.appendFormat("%s) ->", mpp->mName.string());
    }

    HDEBUGLOGD(eDebugLoadBalancing, "%s %p, %s", __func__, src.bufferHandle, after.string());

    return 0;
}

bool ExynosResourceManagerModule::isOverlapped(ExynosDisplay *display, ExynosMPPSource *current,
                                               ExynosMPPSource *compare) {
    int CT = current->mDstImg.y - TDM_OVERLAP_MARGIN;
    CT = (CT < 0) ? 0 : CT;
    int CB = current->mDstImg.y + current->mDstImg.h + TDM_OVERLAP_MARGIN;
    CB = (CB > display->mYres) ? display->mYres : CB;
    int LT = compare->mDstImg.y;
    int LB = compare->mDstImg.y + compare->mDstImg.h;

    if (((LT <= CT && CT <= LB) || (LT <= CB && CB <= LB)) ||
        ((CT <= LT && LT <= CB) || (CT < LB && LB <= CB))) {
        HDEBUGLOGD(eDebugTDM, "%s, current %p and compare %p is overlaped", __func__,
                   current->mSrcImg.bufferHandle, compare->mSrcImg.bufferHandle);
        return true;
    }

    return false;
}

uint32_t ExynosResourceManagerModule::getAmounts(ExynosDisplay *display, ExynosMPP *otfMPP,
                                                 uint32_t currentBlockId, ExynosMPPSource *compare,
                                                 ExynosMPPSource *current,
                                                 std::map<tdm_attr_t, uint32_t> &amounts)
{
    uint32_t blockId = otfMPP->getHWBlockId();
    if ((currentBlockId == blockId) && (isOverlapped(display, current, compare))) {
        String8 log;
        if (hwcCheckDebugMessages(eDebugTDM)) {
            log.appendFormat("%s", otfMPP->mName.string());
        }
        for (auto attr = HWAttrs.begin(); attr != HWAttrs.end(); attr++) {
            uint32_t compareAmount = compare->getHWResourceAmount(attr->first);
            if (hwcCheckDebugMessages(eDebugTDM)) {
                log.appendFormat(", attr %s %d(+ %d)", attr->second.string(), amounts[attr->first],
                                 compareAmount);
            }
            amounts[attr->first] += compareAmount;
        }
        HDEBUGLOGD(eDebugTDM, "%s %s", __func__, log.string());
    }

    return 0;
}
