cmake_minimum_required(VERSION 3.14)

project(XvDisplay LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include (../CommonCMake/CommonProjectOutputSet.cmake)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Widgets)

file(GLOB INCLUDE      include/*)
file(GLOB Graphics     Graphics/*.*)
file(GLOB Roi          Roi/*.*)

add_library(XvDisplay SHARED
    ${INCLUDE}
    ${Graphics}
    ${Roi}
)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/Graphics
    ${CMAKE_CURRENT_SOURCE_DIR}/Roi
    ${CMAKE_CURRENT_SOURCE_DIR}/../XvData/include
)

target_link_libraries(XvDisplay PUBLIC
    XvData
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Widgets
)

target_compile_definitions(XvDisplay PRIVATE XVDISPLAY_LIBRARY)
