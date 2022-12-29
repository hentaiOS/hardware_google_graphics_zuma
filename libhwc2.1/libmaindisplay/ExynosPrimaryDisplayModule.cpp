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

    if (readLineFromFile(mSysfsPath, op_rate_str, '\n') != OK) {
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

int32_t ExynosPrimaryDisplayModule::OperationRateManager::getRefreshRate(const int32_t config_id) {
    constexpr float nsecsPerSec = std::chrono::nanoseconds(1s).count();
    return round(nsecsPerSec / mDisplay->mDisplayConfigs[config_id].vsyncPeriod * 0.1f) * 10;
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
    mDisplayRefreshRate = getRefreshRate(cfg);
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
    Mutex::Autolock lock(mLock);
    OP_MANAGER_LOGD("mode=%d", mode);
    mDisplayPowerMode = static_cast<hwc2_power_mode_t>(mode);
    return updateOperationRateLocked(DispOpCondition::PANEL_SET_POWER);
}

int32_t ExynosPrimaryDisplayModule::OperationRateManager::updateOperationRateLocked(
        const DispOpCondition cond) {
    int32_t ret = HWC2_ERROR_NONE, dbv;

    // skip op rate update under AOD and power off
    if (mDisplayPowerMode != HWC2_POWER_MODE_ON) return ret;

    ATRACE_CALL();
    if (cond == DispOpCondition::SET_DBV) {
        dbv = mDisplayDbv;
    } else {
        dbv = mDisplayLastDbv;
    }

    int32_t desiredOpRate = mDisplayHsOperationRate;
    int32_t curRefreshRate = getRefreshRate(mDisplay->mActiveConfig);
    bool isSteadyLowRefreshRate =
            (mDisplayPeakRefreshRate && mDisplayPeakRefreshRate <= mDisplayNsOperationRate) ||
            mDisplayLowBatteryModeEnabled;
    int32_t effectiveOpRate = 0;

    // check minimal opertion rate needed
    if (isSteadyLowRefreshRate && curRefreshRate <= mDisplayNsOperationRate) {
        desiredOpRate = mDisplayNsOperationRate;
    }
    // check blocking zone
    if (mDisplayLastDbv < mDisplayNsMinDbv || dbv < mDisplayNsMinDbv) {
        desiredOpRate = mDisplayHsOperationRate;
    }
    mDisplayActiveOperationRate = getOperationRate();

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
        if (delta > BRIGHTNESS_DELTA_THRESHOLD) effectiveOpRate = desiredOpRate;
        mDisplayLastDbv = dbv;
        if (effectiveOpRate && (effectiveOpRate != mDisplayActiveOperationRate)) {
            OP_MANAGER_LOGD("brightness delta=%d", delta);
        } else {
            return ret;
        }
    }
    if (effectiveOpRate) ret = setOperationRate(effectiveOpRate);

    OP_MANAGER_LOGI("Op@%d(desired:%d) | Refresh@%d(peak:%d), Battery:%s, DBV:%d(NsMin:%d)",
                    mDisplayActiveOperationRate, desiredOpRate, curRefreshRate,
                    mDisplayPeakRefreshRate, mDisplayLowBatteryModeEnabled ? "Low" : "OK",
                    mDisplayLastDbv, mDisplayNsMinDbv);
    return ret;
}
