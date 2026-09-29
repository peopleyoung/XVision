#include "../XvCamera/Core/VendorAbi.h"
#include <atomic>
#include <cstring>
#include <algorithm>
#include <cstdlib>
using namespace XvCamera::Hardware;
#ifdef _WIN32
#define MOCK_API extern "C" __declspec(dllexport)
#else
#define MOCK_API extern "C" __attribute__((visibility("default")))
#endif
namespace {
std::atomic_int fault{0},outstanding{0},handles{0},writes{0};
struct Handle { double exposure=1000,gain=2; bool started=false; unsigned char pixels[6]={1,2,3,4,5,6}; };
}
MOCK_API void XV_CAMERA_CALL XvMockSetFault(int value) { fault=value; }
MOCK_API int XV_CAMERA_CALL XvMockOutstanding() { return outstanding.load(); }
MOCK_API int XV_CAMERA_CALL XvMockWrites() { return writes.load(); }
MOCK_API int XV_CAMERA_CALL XvMockHandles() { return handles.load(); }
#ifdef XVISION_MOCK_MVS
MOCK_API int XV_CAMERA_CALL MV_CC_Initialize() { return 0; }
MOCK_API int XV_CAMERA_CALL MV_CC_Finalize() { return 0; }
MOCK_API int XV_CAMERA_CALL MV_CC_EnumDevices(unsigned int,Abi::MvsDevices *list)
{
    static Abi::MvsDevicePrefix device{};
    device.transport=1;
    std::strcpy(reinterpret_cast<char*>(device.special.gige.serial),"TEST-MVS-001");
    std::strcpy(reinterpret_cast<char*>(device.special.gige.model),"MVS-Test");
    list->count=1; list->devices[0]=&device; return 0;
}
MOCK_API int XV_CAMERA_CALL MV_CC_CreateHandle(void **handle,const void*) { *handle=new Handle; ++handles; return 0; }
MOCK_API int XV_CAMERA_CALL MV_CC_DestroyHandle(void *handle) { delete static_cast<Handle*>(handle); --handles; return 0; }
MOCK_API int XV_CAMERA_CALL MV_CC_OpenDevice(void*,unsigned int,unsigned short) { return fault==5?int(0x80000203):0; }
MOCK_API int XV_CAMERA_CALL MV_CC_CloseDevice(void*) { return 0; }
MOCK_API int XV_CAMERA_CALL MV_CC_StartGrabbing(void *handle) { static_cast<Handle*>(handle)->started=true; return 0; }
MOCK_API int XV_CAMERA_CALL MV_CC_StopGrabbing(void *handle) { static_cast<Handle*>(handle)->started=false; return 0; }
MOCK_API int XV_CAMERA_CALL MV_CC_GetImageBuffer(void *handle,Abi::MvsFrame *frame,unsigned int)
{
    if(fault==1) return int(0x80000206);
    if(fault==2) return int(0x80000007);
    auto *camera=static_cast<Handle*>(handle);
    if(!camera->started) return int(0x80000003);
    *frame={}; frame->data=camera->pixels; frame->info.width=3; frame->info.height=2;
    frame->info.length=fault==3?1:6; frame->info.pixelType=static_cast<Abi::MvsPixel>(Abi::Mono8);
    ++outstanding; return 0;
}
MOCK_API int XV_CAMERA_CALL MV_CC_FreeImageBuffer(void*,Abi::MvsFrame *frame)
{ std::memset(frame->data,9,6); --outstanding; return 0; }
MOCK_API int XV_CAMERA_CALL MV_CC_ConvertPixelTypeEx(void*,Abi::MvsConvert*) { return int(0x80000001); }
MOCK_API int XV_CAMERA_CALL MV_CC_SetEnumValueByString(void*,const char*,const char*) { return 0; }
MOCK_API int XV_CAMERA_CALL MV_CC_GetFloatValue(void *handle,const char *key,Abi::MvsFloat *value)
{
    *value={}; value->minimum=0; value->maximum=100000;
    value->value=float(std::strcmp(key,"Gain")==0?static_cast<Handle*>(handle)->gain:static_cast<Handle*>(handle)->exposure); return 0;
}
MOCK_API int XV_CAMERA_CALL MV_CC_SetFloatValue(void *handle,const char *key,float value)
{ ++writes; (std::strcmp(key,"Gain")==0?static_cast<Handle*>(handle)->gain:static_cast<Handle*>(handle)->exposure)=value; return 0; }
MOCK_API int XV_CAMERA_CALL MV_CC_SetCommandValue(void*,const char*) { return 0; }
MOCK_API int XV_CAMERA_CALL MV_CC_ClearImageBuffer(void*) { return 0; }
#else
MOCK_API int32_t XV_CAMERA_CALL GXInitLib() { return 0; }
MOCK_API int32_t XV_CAMERA_CALL GXCloseLib() { return 0; }
MOCK_API int32_t XV_CAMERA_CALL GXUpdateDeviceList(uint32_t *count,uint32_t) { *count=1; return 0; }
MOCK_API int32_t XV_CAMERA_CALL GXGetAllDeviceBaseInfo(Abi::GalaxyDevice *info,size_t *size)
{
    if(*size<sizeof(*info)) return -9;
    *info={}; std::strcpy(info->serial,"TEST-GALAXY-001"); std::strcpy(info->model,"Galaxy-Test"); info->type=2;
    *size=sizeof(*info); return 0;
}
MOCK_API int32_t XV_CAMERA_CALL GXOpenDevice(Abi::GalaxyOpen *options,void **handle)
{
    if(fault==5) return -8;
    if(options->mode!=0 || options->access!=4 || std::strcmp(options->content,"TEST-GALAXY-001")) return -5;
    *handle=new Handle; ++handles; return 0;
}
MOCK_API int32_t XV_CAMERA_CALL GXCloseDevice(void *handle) { delete static_cast<Handle*>(handle); --handles; return 0; }
MOCK_API int32_t XV_CAMERA_CALL GXGetInt(void*,int32_t feature,int64_t *value) { if(feature!=Abi::GxPayload) return -12; *value=6; return 0; }
MOCK_API int32_t XV_CAMERA_CALL GXGetEnum(void*,int32_t feature,int64_t *value) { if(feature!=Abi::GxPixelFormat) return -12; *value=Abi::Mono8; return 0; }
MOCK_API int32_t XV_CAMERA_CALL GXSetEnum(void*,int32_t,int64_t) { return 0; }
MOCK_API int32_t XV_CAMERA_CALL GXSendCommand(void *handle,int32_t feature)
{ if(feature==Abi::GxStart) static_cast<Handle*>(handle)->started=true; if(feature==Abi::GxStop) static_cast<Handle*>(handle)->started=false; return 0; }
MOCK_API int32_t XV_CAMERA_CALL GXGetImage(void *handle,Abi::GalaxyFrame *frame,uint32_t)
{
    if(fault==1) return -4;
    if(fault==2) return -14;
    if(!static_cast<Handle*>(handle)->started) return -7;
    if(frame->length<6) return -9;
    std::memcpy(frame->data,static_cast<Handle*>(handle)->pixels,6);
    frame->width=3; frame->height=2; frame->length=fault==3?1:6; frame->status=0; frame->pixelType=Abi::Mono8; return 0;
}
MOCK_API int32_t XV_CAMERA_CALL GXFlushQueue(void*) { return 0; }
MOCK_API int32_t XV_CAMERA_CALL GXGetFloat(void *handle,int32_t feature,double *value)
{ *value=feature==Abi::GxGain?static_cast<Handle*>(handle)->gain:static_cast<Handle*>(handle)->exposure; return 0; }
MOCK_API int32_t XV_CAMERA_CALL GXGetFloatRange(void*,int32_t,Abi::GalaxyFloatRange *range)
{ *range={}; range->maximum=100000; return 0; }
MOCK_API int32_t XV_CAMERA_CALL GXSetFloat(void *handle,int32_t feature,double value)
{ ++writes; (feature==Abi::GxGain?static_cast<Handle*>(handle)->gain:static_cast<Handle*>(handle)->exposure)=value; return 0; }
#endif
