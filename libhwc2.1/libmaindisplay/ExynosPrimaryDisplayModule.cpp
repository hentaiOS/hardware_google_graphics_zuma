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

#include "ExynosPrimaryDisplayModule.h"

#include <cutils/properties.h>

#include "ExynosHWCHelper.h"

#define OP_MANAGER_LOGD(msg, ...)                                                          \
    ALOGD("[%s] OperationRateManager::%s:" msg, mDisplay->mDisplayName.string(), __func__, \
          ##__VA_ARGS__)
#define OP_MANAGER_LOGI(msg, ...)                                                          \
    ALOGI("[%s] OperationRateManager::%s:" msg, mDisplay->mDisplayName.string(), __func__, \
          ##__VA_ARGS__)
#define OP_MANAGER_LOGE(msg, ...)                                                          \
    ALOGE("[%s] OperationRateManager::%s:" msg, mDisplay->mDisplayName.string(), __func__, \
          ##__VA_ARGS__)

using namespace zuma;

ExynosPrimaryDisplayModule::ExynosPrimaryDisplayModule(uint32_t index, ExynosDevice* device,
                                                       const std::string& displayName)
      : gs201::ExynosPrimaryDisplayModule(index, device, displayName) {
    int32_t hs_hz = property_get_int32("vendor.primarydisplay.op.hs_hz", 0);
    int32_t ns_hz = property_get_int32("vendor.primarydisplay.op.ns_hz", 0);

    if (hs_hz && ns_hz) {
        mOperationRateManager = std::make_unique<OperationRateManager>(this, hs_hz, ns_hz);
    }
}

ExynosPrimaryDisplayModule::~ExynosPrimaryDisplayModule ()
{

}

int32_t ExynosPrimaryDisplayModule::validateWinConfigData()
{
    return ExynosDisplay::validateWinConfigData();
}

int32_t ExynosPrimaryDisplayModule::OperationRateManager::getOperationRate() {
    std::string op_rate_str;

    if (mDisplayPowerMode == HWC2_POWER_MODE_DOZE ||
        mDisplayPowerMode == HWC2_POWER_MODE_DOZE_SUSPEND) {
        return LP_OP_RATE;
    } else if (readLineFromFile(mSysfsPath, op_rate_str, '\n') != OK) {
        OP_MANAGER_LOGE("failed to read %s", mSysfsPath.c_str());
        return 0;
    }
    return !op_rate_str.empty() ? std::atoi(op_rate_str.c_str()) : 0;
}

int32_t ExynosPrimaryDisplayModule::OperationRateManager::setOperationRate(const int32_t rate) {
    if (!mDisplayActiveOperationRate || mDisplayActiveOperationRate == rate) return NO_ERROR;

    int32_t ret = writeIntToFile(mSysfsPath.c_str(), rate);
    if (ret == NO_ERROR) {
        mDisplayActiveOperationRate = rate;
        OP_MANAGER_LOGI("succeed to write operation rate %d", rate);
    } else {
        OP_MANAGER_LOGE("failed to write operation rate %d", rate);
    }

    return ret;
}

ExynosPrimaryDisplayModule::OperationRateManager::OperationRateManager(
        ExynosPrimaryDisplay* display, int32_t hsHz, int32_t nsHz)
      : gs201::ExynosPrimaryDisplayModule::OperationRateManager(),
        mDisplay(display),
        mDisplayHsOperationRate(hsHz),
        mDisplayNsOperationRate(nsHz),
        mDisplayPeakRefreshRate(0),
        mDisplayRefreshRate(0),
        mDisplayLastDbv(0),
        mDisplayDbv(0),
        mDisplayPowerMode(HWC2_POWER_MODE_ON),
        mDisplayLowBatteryModeEnabled(false) {
    mDisplayNsMinDbv = property_get_int32("vendor.primarydisplay.op.ns_min_dbv", 0);
    mDisplayActiveOperationRate = mDisplayHsOperationRate;
    mSysfsPath = mDisplay->getPanelSysfsPath(DisplayType::DISPLAY_PRIMARY) + "op_hz";
    OP_MANAGER_LOGI("Op Rate: NS=%d HS=%d NsMinDbv=%d", mDisplayNsOperationRate,
                    mDisplayHsOperationRate, mDisplayNsMinDbv);
}

ExynosPrimaryDisplayModule::OperationRateManager::~OperationRateManager() {}

int32_t ExynosPrimaryDisplayModule::OperationRateManager::onPeakRefreshRate(uint32_t rate) {
    Mutex::Autolock lock(mLock);
    OP_MANAGER_LOGD("rate=%d", rate);
    mDisplayPeakRefreshRate = rate;
    return 0;
}

int32_t ExynosPrimaryDisplayModule::OperationRateManager::onLowPowerMode(bool enabled) {
    Mutex::Autolock lock(mLock);
    OP_MANAGER_LOGD("enabled=%d", enabled);
    mDisplayLowBatteryModeEnabled = enabled;
    return 0;
}

int32_t ExynosPrimaryDisplayModule::OperationRateManager::onConfig(hwc2_config_t cfg) {
    Mutex::Autolock lock(mLock);
    mDisplayRefreshRate = mDisplay->getRefreshRate(cfg);
    OP_MANAGER_LOGD("rate=%d", mDisplayRefreshRate);
    updateOperationRateLocked(DispOpCondition::SET_CONFIG);
    return 0;
}

int32_t ExynosPrimaryDisplayModule::OperationRateManager::onBrightness(uint32_t dbv) {
    Mutex::Autolock lock(mLock);
    if (dbv == 0 || mDisplayLastDbv == dbv) return 0;
    OP_MANAGER_LOGD("dbv=%d", dbv);
    mDisplayDbv = dbv;
    return updateOperationRateLocked(DispOpCondition::SET_DBV);
}

int32_t ExynosPrimaryDisplayModule::OperationRateManager::onPowerMode(int32_t mode) {
    std::string modeName = "Unknown";
    if (mode == HWC2_POWER_MODE_ON) {
        modeName = "On";
    } else if (mode == HWC2_POWER_MODE_OFF) {
        modeName = "Off";
    } else if (mode == HWC2_POWER_MODE_DOZE || mode == HWC2_POWER_MODE_DOZE_SUSPEND) {
        modeName = "LP";
    }

    Mutex::Autolock lock(mLock);
    OP_MANAGER_LOGD("mode=%s", modeName.c_str());
    mDisplayPowerMode = static_cast<hwc2_power_mode_t>(mode);
    return updateOperationRateLocked(DispOpCondition::PANEL_SET_POWER);
}

int32_t ExynosPrimaryDisplayModule::OperationRateManager::updateOperationRateLocked(
        const DispOpCondition cond) {
    int32_t ret = HWC2_ERROR_NONE, dbv;

    ATRACE_CALL();
    if (cond == DispOpCondition::SET_DBV) {
        dbv = mDisplayDbv;
    } else {
        dbv = mDisplayLastDbv;
    }

    int32_t desiredOpRate = mDisplayHsOperationRate;
    int32_t curRefreshRate = mDisplay->getRefreshRate(mDisplay->mActiveConfig);
    bool isSteadyLowRefreshRate =
            (mDisplayPeakRefreshRate && mDisplayPeakRefreshRate <= mDisplayNsOperationRate) ||
            mDisplayLowBatteryModeEnabled;
    int32_t effectiveOpRate = 0;

    // check minimal operation rate needed
    if (isSteadyLowRefreshRate && curRefreshRate <= mDisplayNsOperationRate) {
        desiredOpRate = mDisplayNsOperationRate;
    }
    // check blocking zone
    if (dbv < mDisplayNsMinDbv) {
        desiredOpRate = mDisplayHsOperationRate;
    }

    if (mDisplayPowerMode == HWC2_POWER_MODE_ON) {
        mDisplayActiveOperationRate = getOperationRate();
    } else if (mDisplayPowerMode == HWC2_POWER_MODE_DOZE ||
               mDisplayPowerMode == HWC2_POWER_MODE_DOZE_SUSPEND) {
        mDisplayActiveOperationRate = LP_OP_RATE;
        desiredOpRate = mDisplayActiveOperationRate;
        effectiveOpRate = desiredOpRate;
    } else {
        return ret;
    }

    if (cond == DispOpCondition::SET_CONFIG) {
        curRefreshRate = mDisplayRefreshRate;
        if ((curRefreshRate > mDisplayNsOperationRate) &&
            (curRefreshRate <= mDisplayHsOperationRate))
            effectiveOpRate = mDisplayHsOperationRate;
    } else if (cond == DispOpCondition::PANEL_SET_POWER) {
        effectiveOpRate = desiredOpRate;
    } else if (cond == DispOpCondition::SET_DBV) {
        // TODO: tune brightness delta for different brightness curve and values
        int32_t delta = abs(dbv - mDisplayLastDbv);
        if ((desiredOpRate == mDisplayHsOperationRate) || (delta > BRIGHTNESS_DELTA_THRESHOLD)) {
            effectiveOpRate = desiredOpRate;
        }
        mDisplayLastDbv = dbv;
        if (effectiveOpRate > LP_OP_RATE && (effectiveOpRate != mDisplayActiveOperationRate)) {
            OP_MANAGER_LOGD("brightness delta=%d", delta);
        } else {
            return ret;
        }
    }

    if (!mDisplay->isConfigSettingEnabled() && effectiveOpRate == mDisplayNsOperationRate) {
        OP_MANAGER_LOGI("rate switching is disabled, skip NS op rate update");
        return ret;
    } else if (effectiveOpRate > LP_OP_RATE) {
        ret = setOperationRate(effectiveOpRate);
    }

    OP_MANAGER_LOGI("Op@%d(desired:%d) | Refresh@%d(peak:%d), Battery:%s, DBV:%d(NsMin:%d)",
                    mDisplayActiveOperationRate, desiredOpRate, curRefreshRate,
                    mDisplayPeakRefreshRate, mDisplayLowBatteryModeEnabled ? "Low" : "OK",
                    mDisplayLastDbv, mDisplayNsMinDbv);
    return ret;
}

void ExynosPrimaryDisplayModule::checkPreblendingRequirement() {
    if (!hasDisplayColor()) {
        DISPLAY_LOGD(eDebugTDM, "%s is skipped because of no displaycolor", __func__);
        return;
    }

    String8 log;
    int count = 0;

    auto checkPreblending = [&](const int idx, ExynosMPPSource* mppSrc) -> int {
        auto& dpp = getDppForLayer(mppSrc);
        mppSrc->mNeedPreblending =
                dpp.EotfLut().enable | dpp.Gm().enable | dpp.Dtm().enable | dpp.OetfLut().enable;
        if (hwcCheckDebugMessages(eDebugTDM)) {
            log.appendFormat(" i=%d,pb(%d-%d,%d,%d,%d)", idx, mppSrc->mNeedPreblending,
                             dpp.EotfLut().enable, dpp.Gm().enable, dpp.Dtm().enable,
                             dpp.OetfLut().enable);
        }
        return mppSrc->mNeedPreblending;
    };

    // for client target
    count += checkPreblending(-1, &mClientCompositionInfo);

    // for normal layers
    for (size_t i = 0; i < mLayers.size(); ++i) {
        count += checkPreblending(i, mLayers[i]);
    }
    DISPLAY_LOGD(eDebugTDM, "disp(%d),cnt=%d%s", mDisplayId, count, log.string());
}
