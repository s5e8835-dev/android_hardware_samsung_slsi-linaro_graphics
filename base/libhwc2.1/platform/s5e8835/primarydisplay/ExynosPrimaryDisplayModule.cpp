/*
 * Copyright (C) 2012 The Android Open Source Project
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
#include "ExynosHWCDebug.h"
#include "ExynosLayer.h"

#include <vector>

ExynosPrimaryDisplayModule::ExynosPrimaryDisplayModule(DisplayIdentifier node)
    :    ExynosPrimaryDisplay(node)
{
}

ExynosPrimaryDisplayModule::~ExynosPrimaryDisplayModule () {
}

int32_t ExynosPrimaryDisplayModule::validateWinConfigData()
{
    if (mDisplayConfigPending)
        setActiveConfigInternal(mActiveConfig);

    return ExynosDisplay::validateWinConfigData();
}

int32_t ExynosPrimaryDisplayModule::configureOverlay(ExynosCompositionInfo &compositionInfo) {
    int32_t ret = ExynosDisplay::configureOverlay(compositionInfo);
    int32_t windowIndex = compositionInfo.mWindowIndex;

    if ((windowIndex < 0) || (windowIndex >= (int32_t)mDpuData.configs.size())) {
        DISPLAY_LOGE("%s:: ExynosCompositionInfo(%d) has invalid data, windowIndex(%d)",
                __func__, compositionInfo.mType, windowIndex);
        return -EINVAL;
    }
    exynos_win_config_data &config = mDpuData.configs[windowIndex];

    if (config.fd_idma[2] == -1) {
        int hdrIndex = -1;

        for (size_t i = (size_t)compositionInfo.mFirstIndex; i <= (size_t)compositionInfo.mLastIndex; i++) {
            ExynosLayer *layer = mLayers[i];
            if (layer->mIsHdrLayer == true) {
                if (layer->mExynosCompositionType == HWC2_COMPOSITION_DEVICE) {
                    continue;
                } else {
                    config.hdrLayerDataspace = layer->mDataSpace;
                    hdrIndex = i;
                }
            }
        }
        if (hdrIndex >= 0) {
            /* When GPU target buffer includes HDR layer,
               HDR metadata is set the fd2 of GPU target buffer */
            if (mLayers[hdrIndex]->mMetaParcelFd >= 0)
                config.fd_idma[2] = mLayers[hdrIndex]->mMetaParcelFd;
        }
    }
    return ret;
}

void ExynosPrimaryDisplayModule::resetForDestroyClient() {
    if (!mPlugState)
        return;

    mConfigRequestState = hwc_request_state_t::SET_CONFIG_STATE_NONE;
    mDesiredConfig = 0;
    uint64_t temp = 0;
    setPowerMode(HWC2_POWER_MODE_ON, temp);
    mActiveConfig = mDisplayInterface->getPreferredModeId();
    setActiveConfigInternal(mActiveConfig);
    setVsyncEnabledInternal(HWC2_VSYNC_DISABLE);
    mVsyncCallback.resetVsyncTimeStamp();
    mVsyncCallback.resetDesiredVsyncPeriod();
    mVsyncCallback.enableVSync(0);
}

/*
 * SystemUI keeps the UDFPS dim layer up for as long as the overlay is shown, which would put
 * the panel at mask_brightness behind it on the whole lockscreen. Only engage the fingerprint
 * mask while the pressed (illumination) layer is on screen, and keep the dim layer transparent
 * until then.
 *
 * Screen-off presses show the pressed layer without any dim layer. Engage the mask from here
 * then and hide every other window, so only the sensor lights up.
 */
int ExynosPrimaryDisplayModule::deliverWinConfigData(DevicePresentInfo &presentInfo) {
    ExynosLayer *pressedLayer = nullptr;
    bool hasDimLayer = false;
    for (size_t i = 0; i < mLayers.size(); i++) {
        if (mLayers[i]->mZOrder & UDFPS_PRESSED_LAYER_ZORDER)
            pressedLayer = mLayers[i];
        else if (mLayers[i]->mZOrder == UDFPS_DIM_LAYER_ZORDER)
            hasDimLayer = true;
    }

    ExynosLayer *screenOffPressedLayer = nullptr;
    uint32_t screenOffPressedZOrder = 0;
    if (pressedLayer && !hasDimLayer &&
        (pressedLayer->mExynosCompositionType == HWC2_COMPOSITION_DEVICE)) {
        int32_t pressedWindow = pressedLayer->mWindowIndex;
        if ((pressedWindow >= 0) && (pressedWindow < (int32_t)mDpuData.configs.size())) {
            for (size_t j = 0; j < mDpuData.configs.size(); j++) {
                if ((int32_t)j != pressedWindow)
                    mDpuData.configs[j].plane_alpha = 0;
            }
            /* Let the common code raise the fingerprint mask for this frame only */
            screenOffPressedLayer = pressedLayer;
            screenOffPressedZOrder = pressedLayer->mZOrder;
            pressedLayer->mZOrder = UDFPS_DIM_LAYER_ZORDER;
        }
    }

    std::vector<ExynosLayer *> idleDimLayers;
    if (!pressedLayer) {
        for (size_t i = 0; i < mLayers.size(); i++) {
            ExynosLayer *layer = mLayers[i];
            int32_t windowIndex = layer->mWindowIndex;
            if ((layer->mZOrder != UDFPS_DIM_LAYER_ZORDER) ||
                (layer->mExynosCompositionType != HWC2_COMPOSITION_DEVICE) ||
                (windowIndex < 0) || (windowIndex >= (int32_t)mDpuData.configs.size()))
                continue;
            mDpuData.configs[windowIndex].plane_alpha = 0;
            /* Hide it from the fingerprint mask lookup for this frame only */
            layer->mZOrder = UDFPS_DIM_LAYER_ZORDER - 1;
            idleDimLayers.push_back(layer);
        }
    }

    int ret = ExynosPrimaryDisplay::deliverWinConfigData(presentInfo);

    for (ExynosLayer *layer : idleDimLayers)
        layer->mZOrder = UDFPS_DIM_LAYER_ZORDER;
    if (screenOffPressedLayer)
        screenOffPressedLayer->mZOrder = screenOffPressedZOrder;

    return ret;
}
