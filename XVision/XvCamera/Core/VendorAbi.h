#pragma once
// Minimal C ABI declarations, verified against the vendor-distributed headers.
// See doc/开发工作/实机相机接入/SDK接口核验.md. No vendor binaries are bundled.
#include <cstdint>
#include <cstddef>
#ifdef _WIN32
#define XV_CAMERA_CALL __stdcall
#else
#define XV_CAMERA_CALL
#endif
namespace XvCamera { namespace Hardware { namespace Abi {
#pragma pack(push,8)
struct MvsGigE
{
    uint32_t ipOptions,ipCurrent,ip,subnet,gateway;
    unsigned char vendor[32],model[32],version[32],specific[48],serial[16],user[16];
    uint32_t netExport,reserved[4];
};
struct MvsUsb
{
    uint8_t endpoints[4]; uint16_t vendorId,productId; uint32_t number;
    unsigned char guid[64],vendor[64],model[64],family[64],version[64],manufacturer[64],serial[64],user[64];
    uint32_t bcdUsb,address,reserved[2];
};
// Prefix view only; enumeration retains ownership of the complete SDK object.
// Never allocate, copy or pass sizeof(MvsDevicePrefix) to the SDK.
struct MvsDevicePrefix
{
    uint16_t major,minor; uint32_t macHigh,macLow,transport,reserved[4];
    union { MvsGigE gige; MvsUsb usb; } special;
};
struct MvsDevices { uint32_t count; void *devices[256]; };
// Match the vendor header's platform-specific enum range, including custom PFNC.
enum MvsPixel
{
#ifdef _WIN32
    MvsUndefined=0xffffffffu,
#else
    MvsUndefined=-1,
#endif
    MvsCustom=0x80000000u
};
struct MvsFrameInfo
{
    uint16_t width,height; MvsPixel pixelType;
    uint32_t frameNumber,deviceTimeHigh,deviceTimeLow,reserved0; int64_t hostTime;
    uint32_t length,second,cycle,offset; float gain,exposure;
    uint32_t brightness,red,green,blue,frameCounter,triggerIndex,input,output;
    uint16_t offsetX,offsetY,chunkWidth,chunkHeight;
    uint32_t lostPackets,chunkCount; int64_t chunkList;
    uint32_t extendedWidth,extendedHeight; uint64_t extendedLength;
    uint32_t reserved1,subImageCount; int64_t subImages,user;
    uint32_t firstEncoder,lastEncoder,reserved[24];
};
struct MvsFrame { unsigned char *data; MvsFrameInfo info; uint32_t reserved[16]; };
struct MvsConvert
{
    uint32_t width,height; MvsPixel sourceType; unsigned char *source; uint32_t sourceLength;
    MvsPixel destinationType; unsigned char *destination; uint32_t destinationLength,destinationCapacity,reserved[4];
};
struct MvsFloat { float value,maximum,minimum; uint32_t reserved[4]; };
struct MvsEnum { uint32_t value,count,supported[64],reserved[4]; };
struct GalaxyDevice
{
    char vendor[32],model[32],serial[32],display[132],id[68],user[68];
    int32_t access,type; char reserved[300];
};
struct GalaxyOpen { char *content; int32_t mode,access; };
struct GalaxyFrame
{
    int32_t status; void *data; int32_t width,height,pixelType,length;
    uint64_t id,timestamp; int32_t reserved[3];
};
struct GalaxyFloatRange { double minimum,maximum,increment; char unit[8]; bool incrementValid; int8_t reserved[31]; };
#pragma pack(pop)
constexpr int32_t GxPayload=0x10000000|2000;
constexpr int32_t GxPixelFormat=0x30000000|1014;
constexpr int32_t GxAcquisitionMode=0x30000000|3000;
constexpr int32_t GxStart=0x70000000|3001,GxStop=0x70000000|3002;
constexpr int32_t GxTriggerMode=0x30000000|3005,GxTriggerSource=0x30000000|3013;
constexpr int32_t GxTrigger=0x70000000|3006;
constexpr int32_t GxExposure=0x20000000|3009,GxGain=0x20000000|5011;
constexpr int32_t GxExposureAuto=0x30000000|3010,GxGainAuto=0x30000000|5000;
constexpr int32_t Mono8=0x01080001,Rgb8=0x02180014,Bgr8=0x02180015;
} } }
