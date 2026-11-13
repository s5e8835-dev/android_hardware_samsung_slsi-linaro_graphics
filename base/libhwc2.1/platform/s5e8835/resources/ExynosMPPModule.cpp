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

#include "ExynosMPPModule.h"
#include "ExynosHWCDebug.h"
#include "ExynosResourceManager.h"
#include "ExynosResourceRestriction.h"

ExynosMPPModule::ExynosMPPModule(uint32_t physicalType, uint32_t logicalType,
        const char *name, uint32_t physicalIndex, uint32_t logicalIndex,
        uint32_t preAssignInfo, uint32_t mppType)
    : ExynosMPP(physicalType, logicalType, name, physicalIndex, logicalIndex, preAssignInfo, mppType)
{
}

ExynosMPPModule::~ExynosMPPModule()
{
}

uint32_t ExynosMPPModule::getDstWidthAlign(struct exynos_image &dst)
{
    if (((dst.exynosFormat == HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_SPN_S10B) ||
         (dst.exynosFormat == HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_SP_M_S10B)) &&
        (mPhysicalType == MPP_MSC))
        return 4;

    return ExynosMPP::getDstWidthAlign(dst);
}

static bool isSdrGammaTransfer(uint32_t transfer)
{
    return (transfer == HAL_DATASPACE_TRANSFER_UNSPECIFIED) ||
           (transfer == HAL_DATASPACE_TRANSFER_SRGB) ||
           (transfer == HAL_DATASPACE_TRANSFER_SMPTE_170M) ||
           (transfer == HAL_DATASPACE_TRANSFER_GAMMA2_2);
}

/*
 * SDR video (e.g. BT.709 / SMPTE 170M) shown on an sRGB output only differs in the transfer
 * label, which needs no wide colour gamut conversion. Without this, the MSC refuses such layers
 * and every video frame gets composed by the GPU instead.
 */
bool ExynosMPPModule::isDataspaceSupportedByMPP(struct exynos_image &src, struct exynos_image &dst)
{
    uint32_t srcStandard = (src.dataSpace & HAL_DATASPACE_STANDARD_MASK);
    uint32_t dstStandard = (dst.dataSpace & HAL_DATASPACE_STANDARD_MASK);
    uint32_t srcTransfer = (src.dataSpace & HAL_DATASPACE_TRANSFER_MASK);
    uint32_t dstTransfer = (dst.dataSpace & HAL_DATASPACE_TRANSFER_MASK);

    if (!hasHdrInfo(src) && (srcStandard == dstStandard) &&
        isSdrGammaTransfer(srcTransfer) && isSdrGammaTransfer(dstTransfer))
        return true;

    return ExynosMPP::isDataspaceSupportedByMPP(src, dst);
}

uint32_t ExynosMPPModule::getSrcMaxCropWidth(struct exynos_image &src)
{
    if ((mPhysicalType == MPP_DPP_VGFS) && (src.compressionInfo.type == COMP_TYPE_AFBC))
        return VGFS_AFBC_WIDTH_LIMIT;
    else
        return ExynosMPP::getSrcMaxCropWidth(src);
    return ExynosMPP::getSrcMaxCropWidth(src);
}
