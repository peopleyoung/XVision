if(NOT XVISION_ENABLE_ONNXRUNTIME)
    return()
endif()
if(TARGET XVision::ONNXRuntime)
    return()
endif()

set(XVISION_ONNXRUNTIME_ROOT "" CACHE PATH
    "ONNX Runtime installation root containing include/, lib/ and bin/")
if(XVISION_ONNXRUNTIME_ROOT STREQUAL "")
    message(FATAL_ERROR
        "XVISION_ENABLE_ONNXRUNTIME is ON but XVISION_ONNXRUNTIME_ROOT is empty")
endif()

find_path(XVISION_ONNXRUNTIME_INCLUDE_DIR
    NAMES onnxruntime_cxx_api.h
    PATHS "${XVISION_ONNXRUNTIME_ROOT}/include"
    NO_DEFAULT_PATH
)
find_library(XVISION_ONNXRUNTIME_LIBRARY
    NAMES onnxruntime
    PATHS "${XVISION_ONNXRUNTIME_ROOT}/lib"
    NO_DEFAULT_PATH
)
if(NOT XVISION_ONNXRUNTIME_INCLUDE_DIR
        OR NOT EXISTS "${XVISION_ONNXRUNTIME_INCLUDE_DIR}/onnxruntime_c_api.h"
        OR NOT XVISION_ONNXRUNTIME_LIBRARY)
    message(FATAL_ERROR
        "ONNX Runtime SDK is incomplete under '${XVISION_ONNXRUNTIME_ROOT}'; "
        "expected include/onnxruntime_cxx_api.h, include/onnxruntime_c_api.h and "
        "lib/onnxruntime import library")
endif()

find_file(XVISION_ONNXRUNTIME_RUNTIME_DLL
    NAMES onnxruntime.dll
    PATHS "${XVISION_ONNXRUNTIME_ROOT}/bin" "${XVISION_ONNXRUNTIME_ROOT}/lib"
    NO_DEFAULT_PATH
)
if(WIN32 AND NOT XVISION_ONNXRUNTIME_RUNTIME_DLL)
    message(FATAL_ERROR
        "ONNX Runtime SDK is missing onnxruntime.dll under '${XVISION_ONNXRUNTIME_ROOT}/bin'")
endif()
if(WIN32)
    add_library(XVision::ONNXRuntime SHARED IMPORTED GLOBAL)
    set_target_properties(XVision::ONNXRuntime PROPERTIES
        IMPORTED_LOCATION "${XVISION_ONNXRUNTIME_RUNTIME_DLL}"
        IMPORTED_IMPLIB "${XVISION_ONNXRUNTIME_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${XVISION_ONNXRUNTIME_INCLUDE_DIR}"
    )
else()
    add_library(XVision::ONNXRuntime UNKNOWN IMPORTED GLOBAL)
    set_target_properties(XVision::ONNXRuntime PROPERTIES
        IMPORTED_LOCATION "${XVISION_ONNXRUNTIME_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${XVISION_ONNXRUNTIME_INCLUDE_DIR}"
    )
endif()
set(XVISION_ONNXRUNTIME_RUNTIME_DLLS "${XVISION_ONNXRUNTIME_RUNTIME_DLL}" CACHE INTERNAL
    "ONNX Runtime files required beside XVision binaries")
