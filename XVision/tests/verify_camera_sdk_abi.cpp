#include "CameraParams.h"
#include "GxIAPI.h"
#include "../XvCamera/Core/VendorAbi.h"
#include <cstddef>
#include <iostream>
using namespace XvCamera::Hardware::Abi;
#define SIZE(a,b) static_assert(sizeof(a)==sizeof(b),#a)
#define OFFSET(a,x,b,y) static_assert(offsetof(a,x)==offsetof(b,y),#a "." #x)
SIZE(MvsGigE,MV_GIGE_DEVICE_INFO); SIZE(MvsUsb,MV_USB3_DEVICE_INFO);
SIZE(MvsDevices,MV_CC_DEVICE_INFO_LIST); SIZE(MvsFrameInfo,MV_FRAME_OUT_INFO_EX);
SIZE(MvsFrame,MV_FRAME_OUT); SIZE(MvsConvert,MV_CC_PIXEL_CONVERT_PARAM_EX);
SIZE(MvsFloat,MVCC_FLOATVALUE); SIZE(MvsEnum,MVCC_ENUMVALUE);
SIZE(GalaxyDevice,GX_DEVICE_BASE_INFO); SIZE(GalaxyOpen,GX_OPEN_PARAM);
SIZE(GalaxyFrame,GX_FRAME_DATA); SIZE(GalaxyFloatRange,GX_FLOAT_RANGE);
OFFSET(MvsDevicePrefix,transport,MV_CC_DEVICE_INFO,nTLayerType);
OFFSET(MvsDevicePrefix,special,MV_CC_DEVICE_INFO,SpecialInfo);
OFFSET(MvsFrameInfo,extendedWidth,MV_FRAME_OUT_INFO_EX,nExtendWidth);
OFFSET(MvsFrameInfo,extendedLength,MV_FRAME_OUT_INFO_EX,nFrameLenEx);
OFFSET(MvsFrameInfo,reserved,MV_FRAME_OUT_INFO_EX,nReserved);
OFFSET(MvsConvert,destination,MV_CC_PIXEL_CONVERT_PARAM_EX,pDstBuffer);
OFFSET(GalaxyFrame,data,GX_FRAME_DATA,pImgBuf);
OFFSET(GalaxyFrame,id,GX_FRAME_DATA,nFrameID);
OFFSET(GalaxyDevice,type,GX_DEVICE_BASE_INFO,deviceClass);
static_assert(GxExposure==GX_FLOAT_EXPOSURE_TIME);
static_assert(GxGain==GX_FLOAT_GAIN);
static_assert(GxTriggerMode==GX_ENUM_TRIGGER_MODE);
static_assert(GxTriggerSource==GX_ENUM_TRIGGER_SOURCE);
static_assert(GxTrigger==GX_COMMAND_TRIGGER_SOFTWARE);
static_assert(GxPayload==GX_INT_PAYLOAD_SIZE);
int main() { std::cout << "Vendor ABI checks passed\n"; }
