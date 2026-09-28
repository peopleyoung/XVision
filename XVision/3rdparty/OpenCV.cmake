if(NOT XVISION_ENABLE_OPENCV)
    return()
endif()
if(TARGET XVision::OpenCV)
    return()
endif()

set(XVISION_OPENCV_ROOT "" CACHE PATH
    "OpenCV installation root containing include/, lib/ and bin/")
if(XVISION_OPENCV_ROOT STREQUAL "")
    message(FATAL_ERROR
        "XVISION_ENABLE_OPENCV is ON but XVISION_OPENCV_ROOT is empty")
endif()

find_path(XVISION_OPENCV_INCLUDE_DIR
    NAMES opencv2/core.hpp
    PATHS "${XVISION_OPENCV_ROOT}/include"
    NO_DEFAULT_PATH
)
find_library(XVISION_OPENCV_WORLD_LIBRARY
    NAMES opencv_world opencv_world4100 opencv_world4100d opencv_world4110
    PATHS "${XVISION_OPENCV_ROOT}/lib"
    NO_DEFAULT_PATH
)
find_file(XVISION_OPENCV_RUNTIME_DLL
    NAMES opencv_world4100.dll opencv_world4110.dll opencv_world.dll
    PATHS "${XVISION_OPENCV_ROOT}/bin" "${XVISION_OPENCV_ROOT}/lib"
    NO_DEFAULT_PATH
)
set(_xvision_opencv_required_headers
    core.hpp
    imgproc.hpp
    features2d.hpp
    xfeatures2d.hpp
    calib3d.hpp
    photo.hpp
    objdetect.hpp
    stitching.hpp
    ml.hpp
    dnn_superres.hpp
    videoio.hpp
    video/background_segm.hpp
)
set(_xvision_opencv_headers_complete TRUE)
foreach(_header IN LISTS _xvision_opencv_required_headers)
    if(NOT XVISION_OPENCV_INCLUDE_DIR
            OR NOT EXISTS "${XVISION_OPENCV_INCLUDE_DIR}/opencv2/${_header}")
        set(_xvision_opencv_headers_complete FALSE)
    endif()
endforeach()
if(NOT _xvision_opencv_headers_complete OR NOT XVISION_OPENCV_WORLD_LIBRARY)
    message(FATAL_ERROR
        "OpenCV SDK is incomplete under '${XVISION_OPENCV_ROOT}'; "
        "expected the Phase 3/4 core/imgproc/features2d/xfeatures2d/calib3d/photo/"
        "objdetect/stitching/ml/dnn_superres/videoio/video headers and a lib/opencv_world import library")
endif()

if(WIN32 AND NOT XVISION_OPENCV_RUNTIME_DLL)
    message(FATAL_ERROR
        "OpenCV SDK is missing its runtime DLL under '${XVISION_OPENCV_ROOT}/bin'")
endif()
if(WIN32)
    add_library(XVision::OpenCV SHARED IMPORTED GLOBAL)
    set_target_properties(XVision::OpenCV PROPERTIES
        IMPORTED_LOCATION "${XVISION_OPENCV_RUNTIME_DLL}"
        IMPORTED_IMPLIB "${XVISION_OPENCV_WORLD_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${XVISION_OPENCV_INCLUDE_DIR}"
    )
else()
    add_library(XVision::OpenCV UNKNOWN IMPORTED GLOBAL)
    set_target_properties(XVision::OpenCV PROPERTIES
        IMPORTED_LOCATION "${XVISION_OPENCV_WORLD_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${XVISION_OPENCV_INCLUDE_DIR}"
    )
endif()
set(_xvision_opencv_runtime_dlls "${XVISION_OPENCV_RUNTIME_DLL}")
if(WIN32)
    file(GLOB _xvision_opencv_video_runtime_dlls
        "${XVISION_OPENCV_ROOT}/bin/opencv_videoio_ffmpeg*.dll")
    list(APPEND _xvision_opencv_runtime_dlls ${_xvision_opencv_video_runtime_dlls})
endif()
set(XVISION_OPENCV_RUNTIME_DLLS "${_xvision_opencv_runtime_dlls}" CACHE INTERNAL
    "OpenCV runtime files required beside XVision binaries")
