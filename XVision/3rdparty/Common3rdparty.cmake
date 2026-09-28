set(HALCON_INC_DIR 3rdparty/halcon/include)
set(HALCONCPP_INC_DIR 3rdparty/halcon/include/halconcpp)

list(APPEND HALCON_LIB
    halcon
    halconcpp
)

set(HALCON_LINK_DIR  3rdparty/halcon/lib)

include(${CMAKE_CURRENT_LIST_DIR}/OpenCV.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/OnnxRuntime.cmake)
