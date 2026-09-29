# SDK interface verification

Date: 2026-09-29. Public camera headers remain vendor-neutral. Core/VendorAbi.h contains only the C declarations needed by the dynamic adapters; no SDK implementation or driver is redistributed.

Vendor-distributed declarations were inspected from archived SDK headers, not inferred from wrapper behavior:

- MVS CameraParams.h: Git blob 54bf8e41ca0dfe13f5b0f4c640789c962c1d902f, MvCameraControl.h d0cb37d1064e735c3f17a86b18566d9e623871e5, PixelType.h 80afed404d9011adee82fb30f755dce4c44a7c50; archive repository YKSilvery/HikRobot-camera-MVS-sdk-ROS2, src/camera/third_party/include/.
- Galaxy GxIAPI.h version 1.19.2301.9311 (2023-01-31): Git blob bfb271c7391398ab4c8e85cd7b1a70ca6542163e; GxPixelFormat.h 041f53d2b48360a8f243a1877b8b967970856477; archive repository Islatri/gxci, doc/inc/.

The manufacturers' download pages rejected automated retrieval (HTTP 403); these archived original headers were used for declarations only. No claims are made about latest SDK versions. Install a vendor-supported SDK for the actual camera.

The MVS pixel enum includes custom PFNC values and a platform-dependent undefined value. GCC chooses a wider enum on Linux for this header, while Windows uses 32 bits. Mirror the actual enum range rather than unconditionally substituting int32_t. tests/verify_camera_sdk_abi.cpp verifies sizes, field offsets and feature constants against the original headers. It is an optional developer check requiring those headers, independent from hardware acceptance.

MVS acquisition uses GetImageBuffer/FreeImageBuffer paired by RAII, with ConvertPixelTypeEx for non-packed formats. SDK-owned enumeration objects are viewed only as prefixes and passed unchanged to CreateHandle while holding the enumeration mutex. They are never copied using a truncated size.

Galaxy uses OpenDevice by serial number and exclusive access. Payload size is checked before allocating the caller-owned GXGetImage buffer. Frame status and byte lengths are checked before publishing a QImage. Features use the IDs in GxIAPI.h; software triggering is sent once per frame request, not per timeout slice.

Both SDKs are loaded dynamically. Missing entry points or unavailable drivers produce an actionable diagnostic. The in-tree mock SDKs exercise load, discovery, open, parameters, successful frame copy, timeout, disconnect, corrupt frame, release, failed-open cleanup and reopen; they do not establish real device compatibility.
