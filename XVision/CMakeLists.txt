cmake_minimum_required(VERSION 3.21)

project(XVision VERSION 0.1 LANGUAGES CXX)

option(BUILD_TESTING "Build XVision first-party tests" OFF)
option(XVISION_BUILD_COMMON_USING "Build bundled CommonUsing libraries" ON)
option(XVISION_BUILD_SYSTEM_PLUGIN "Build the built-in system operator plugin" ON)
option(XVISION_ENABLE_OPENCV "Enable OpenCV-backed operator adapters" OFF)
option(XVISION_ENABLE_ONNXRUNTIME "Enable ONNX Runtime-backed operator adapters" OFF)
include(CTest)

include(${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/OpenCV.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/OnnxRuntime.cmake)

if(XVISION_BUILD_COMMON_USING)
    add_subdirectory(CommonUsing)
endif()

add_subdirectory(XvData)  #项目数据库工程
add_subdirectory(XvCamera)  #通用相机接口及模拟相机
add_subdirectory(XvTokenMsg)  #项目Token消息工程
add_subdirectory(XvUtils) #项目工具库工程
add_subdirectory(XvDisplay)  #图像交互库工程
add_subdirectory(XvCore)  #核心库工程
if(XVISION_BUILD_SYSTEM_PLUGIN)
    add_subdirectory(XvFuncCollection/XvFuncSystem) #系统工具库
endif()
add_subdirectory(XVision) #App主工程

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
