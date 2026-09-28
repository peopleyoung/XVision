cmake_minimum_required(VERSION 3.14)

project(XvFuncSystem LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include (../../CommonCMake/CommonXvFuncOutputSet.cmake)
include (../../CommonUsing/CommonUsing.cmake)
include (../../3rdparty/Common3rdparty.cmake)


#寻找QT库
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Network SerialBus SerialPort Sql Widgets Xml)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Network SerialBus SerialPort Sql Widgets Xml)

#[[**************添加链接库**************]]
#添加QT库
list(APPEND _LIBS
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Network
    Qt${QT_VERSION_MAJOR}::SerialBus
    Qt${QT_VERSION_MAJOR}::SerialPort
    Qt${QT_VERSION_MAJOR}::Sql
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Xml
)
#添加工程库
list(APPEND _LIBS
    XvData #数据库
    XvDisplay #图像交互库
    XvTokenMsg #Token消息库
    XvUtils #工具库
    XvCamera #通用相机接口
)

#添加公共库
list(APPEND _LIBS
    XWidget         #控件库
    XLog            #日志库
    XConcurrent     #多线程管理库
)
#添加第三方库
if(XVISION_ENABLE_OPENCV)
    list(APPEND _LIBS XVision::OpenCV)
endif()
if(XVISION_ENABLE_ONNXRUNTIME)
    list(APPEND _LIBS XVision::ONNXRuntime)
endif()
#[[**************添加包含目录**************]]

#添加工程包含目录
list(APPEND _INCLUDE_DIR
    ../../XvCore/include
    ../../XvTokenMsg/include
    ../../XvUtils/include
    ../../XvData/include
    ../../XvDisplay/include
    ../../XvCamera/include
)
#添加第三方库
list(APPEND _INCLUDE_DIR
    ../../${XWidget_Inc}
    ../../${XLog_Inc}
    ../../${XLanguage_Inc}
    ../../${XConcurrent_Inc}
)


#[[**************添加连接库文件路径**************]]

#添加依赖库目录
list(APPEND _LINK_DIR
    ../../${COMMON_USING_LINK_DIR}
)

link_directories(${_LINK_DIR})


#[[**************添加工程文件**************]]
file(GLOB INCLUDE include/*.*)
file(GLOB SRC src/*.*)
file(GLOB Model Model/*.*)
file(GLOB Def Def/*.*)
file(GLOB Helper Helper/*.*)
file(GLOB Interop Interop/*.*)

file(GLOB ImageAcquisition ImageAcquisition/*.*)
file(GLOB DataProcessing DataProcessing/*.*)
file(GLOB Location Location/*.*)
file(GLOB Detection Detection/*.*)
file(GLOB Segmentation Segmentation/*.*)
file(GLOB ImageProcessing ImageProcessing/*.*)
file(GLOB MorphologyDetection MorphologyDetection/*.*)
file(GLOB Onnx Onnx/*.*)
file(GLOB TemplateRectification TemplateRectification/*.*)
file(GLOB Measurement Measurement/*.*)
file(GLOB Record Record/*.*)
file(GLOB Communication Communication/*.*)
file(GLOB User User/*.*)
file(GLOB Logic Logic/*.*)
file(GLOB Other Other/*.*)


#设置工程文件
set(PROJECT_SOURCES
    ${INCLUDE}
    ${SRC}

    ${Def}
    ${Model}
    ${Helper}
    ${Interop}

    ${DataProcessing}
    ${ImageAcquisition}
    ${Location}
    ${Detection}
    ${Segmentation}
    ${ImageProcessing}
    ${MorphologyDetection}
    ${Onnx}
    ${TemplateRectification}
    ${Measurement}
    ${Record}
    ${Communication}
    ${User}
    ${Logic}
    ${Other}

)
set(PROJECT_RES
    res/XvFuncSystemRes.qrc
)
add_library(XvFuncSystem SHARED
    ${PROJECT_SOURCES}
    ${PROJECT_RES}
)

#添加当前工程包含
list(APPEND _INCLUDE_DIR
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/Model
    ${CMAKE_CURRENT_SOURCE_DIR}/Helper
    ${CMAKE_CURRENT_SOURCE_DIR}/Interop
    ${CMAKE_CURRENT_SOURCE_DIR}/Def

    ${CMAKE_CURRENT_SOURCE_DIR}/ImageAcquisition
    ${CMAKE_CURRENT_SOURCE_DIR}/Location
    ${CMAKE_CURRENT_SOURCE_DIR}/Detection
    ${CMAKE_CURRENT_SOURCE_DIR}/Segmentation
    ${CMAKE_CURRENT_SOURCE_DIR}/ImageProcessing
    ${CMAKE_CURRENT_SOURCE_DIR}/MorphologyDetection
    ${CMAKE_CURRENT_SOURCE_DIR}/Onnx
    ${CMAKE_CURRENT_SOURCE_DIR}/TemplateRectification
    ${CMAKE_CURRENT_SOURCE_DIR}/Measurement
    ${CMAKE_CURRENT_SOURCE_DIR}/Record
    ${CMAKE_CURRENT_SOURCE_DIR}/Communication
    ${CMAKE_CURRENT_SOURCE_DIR}/DataProcessing
    ${CMAKE_CURRENT_SOURCE_DIR}/User
    ${CMAKE_CURRENT_SOURCE_DIR}/Logic
    ${CMAKE_CURRENT_SOURCE_DIR}/Other
)

#[[**************链接-包含**************]]

#包含目录
include_directories(${_INCLUDE_DIR})

#连接[第三方库/内部库] 动态
# Public operator headers expose XvCore types and inline translation calls.
target_include_directories(XvFuncSystem PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../${XLanguage_Inc}"
)
target_link_libraries(XvFuncSystem
    PUBLIC XvCore XLanguage
    PRIVATE ${_LIBS}
)


target_compile_definitions(XvFuncSystem PRIVATE XVFUNCSYSTEM_LIBRARY)
# Keep Windows SDK macros out of public numeric-limits consumers.
if(WIN32)
    target_compile_definitions(XvFuncSystem PUBLIC NOMINMAX)
endif()
if(XVISION_ENABLE_OPENCV)
    target_compile_definitions(XvFuncSystem PRIVATE XVISION_ENABLE_OPENCV=1)
endif()
if(XVISION_ENABLE_ONNXRUNTIME)
    target_compile_definitions(XvFuncSystem PRIVATE XVISION_ENABLE_ONNXRUNTIME=1)
endif()

add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")

# Deploy enabled open-source backend runtimes beside the application.
if(WIN32)
    if(XVISION_ENABLE_OPENCV)
        foreach(OPENCV_DLL IN LISTS XVISION_OPENCV_RUNTIME_DLLS)
            add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${OPENCV_DLL}"
                    $<TARGET_FILE_DIR:${PROJECT_NAME}>/..
            )
        endforeach()
    endif()
    if(XVISION_ENABLE_ONNXRUNTIME)
        foreach(ONNXRUNTIME_DLL IN LISTS XVISION_ONNXRUNTIME_RUNTIME_DLLS)
            add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${ONNXRUNTIME_DLL}"
                    $<TARGET_FILE_DIR:${PROJECT_NAME}>/..
            )
        endforeach()
    endif()
    foreach(_qt_runtime_module Network SerialPort SerialBus)
        if(TARGET Qt${QT_VERSION_MAJOR}::${_qt_runtime_module})
            add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    $<TARGET_FILE:Qt${QT_VERSION_MAJOR}::${_qt_runtime_module}>
                    $<TARGET_FILE_DIR:${PROJECT_NAME}>/..
            )
        endif()
    endforeach()
    if(TARGET Qt${QT_VERSION_MAJOR}::QSQLiteDriverPlugin)
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory
                $<TARGET_FILE_DIR:${PROJECT_NAME}>/../sqldrivers
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:Qt${QT_VERSION_MAJOR}::QSQLiteDriverPlugin>
                $<TARGET_FILE_DIR:${PROJECT_NAME}>/../sqldrivers
        )
    else()
        message(WARNING
            "Qt QSQLiteDriverPlugin target is unavailable; deploy qsqlite manually")
    endif()
endif()
