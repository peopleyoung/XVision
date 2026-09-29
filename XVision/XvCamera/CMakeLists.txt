cmake_minimum_required(VERSION 3.14)

project(XvCamera LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(../CommonCMake/CommonProjectOutputSet.cmake)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Gui)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Gui)

file(GLOB INCLUDE include/*.*)
file(GLOB CORE Core/*.*)

add_library(${PROJECT_NAME} SHARED
    ${INCLUDE}
    ${CORE}
)

target_include_directories(${PROJECT_NAME} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(${PROJECT_NAME} PUBLIC
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Gui
)

target_compile_definitions(${PROJECT_NAME} PRIVATE XVCAMERA_LIBRARY)
add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")

find_package(Threads REQUIRED)
target_link_libraries(XvCamera PRIVATE Threads::Threads)
if(WIN32)
    target_link_libraries(XvCamera PRIVATE mfplat mfreadwrite mfuuid mf ole32)
    target_compile_definitions(XvCamera PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
endif()
