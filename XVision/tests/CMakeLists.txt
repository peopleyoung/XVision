find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Gui Network SerialBus SerialPort Sql Test Widgets Xml)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Gui Network SerialBus SerialPort Sql Test Widgets Xml)
include(../3rdparty/Common3rdparty.cmake)

# Keep Qt Test output even when Windows GUI startup/output capture fails.
set(XVISION_QT_TEST_LOG_DIR "${CMAKE_BINARY_DIR}/Testing/QtTest")
file(MAKE_DIRECTORY "${XVISION_QT_TEST_LOG_DIR}")

set(CMAKE_AUTOMOC ON)

set(VISIONMASTER_MATRIX
    "${CMAKE_CURRENT_SOURCE_DIR}/../../doc/开发工作/VisionMaster算子兼容矩阵.csv")
set(VISIONMASTER_MATRIX_VERIFIER
    "${CMAKE_CURRENT_SOURCE_DIR}/../scripts/VerifyVisionMasterMatrix.cmake")
set(VISIONMASTER_GENERATED_DOCUMENT
    "${CMAKE_CURRENT_SOURCE_DIR}/../../doc/开发工作/VisionMaster全算子适配清单.md")

add_test(NAME visionmaster_matrix
    COMMAND ${CMAKE_COMMAND}
        -DMATRIX=${VISIONMASTER_MATRIX}
        -DVERIFIER=${VISIONMASTER_MATRIX_VERIFIER}
        -DTEST_BINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}/visionmaster-matrix-tests
        -P ${CMAKE_CURRENT_SOURCE_DIR}/../scripts/TestVisionMasterMatrix.cmake
)
add_test(NAME visionmaster_matrix_document
    COMMAND ${CMAKE_COMMAND}
        -DMATRIX=${VISIONMASTER_MATRIX}
        -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/VisionMaster全算子适配清单.md
        -DEXPECTED_DOCUMENT=${VISIONMASTER_GENERATED_DOCUMENT}
        -P ${VISIONMASTER_MATRIX_VERIFIER}
)

add_test(NAME visionmaster_sdk_cmake
    COMMAND ${CMAKE_COMMAND}
        -DXVISION_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}/..
        -DFIXTURE_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}/cmake/sdk_adapter
        -DTEST_BINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}/visionmaster-sdk-tests
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/TestSdkAdapters.cmake
)

add_executable(XvVisionMasterDataTests
    tst_visionmasterdata.cpp
)
set_target_properties(XvVisionMasterDataTests PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)
target_include_directories(XvVisionMasterDataTests PRIVATE
    ../XvData/include
    ../XvCore/include
)
target_link_libraries(XvVisionMasterDataTests PRIVATE
    XvCore
    XvData
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Gui
    Qt${QT_VERSION_MAJOR}::Test
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Xml
)
add_test(NAME visionmaster_data
    COMMAND ${CMAKE_COMMAND} -E env
        "QT_QPA_PLATFORM=offscreen"
        "PATH=$<TARGET_FILE_DIR:XvVisionMasterDataTests>$<SEMICOLON>$<TARGET_FILE_DIR:XVision>$<SEMICOLON>$ENV{QTDIR}/bin$<SEMICOLON>$ENV{PATH}"
        $<TARGET_FILE:XvVisionMasterDataTests>
        -o "${XVISION_QT_TEST_LOG_DIR}/visionmaster_data.txt,txt"
        -o "-,txt"
)

set(XVISION_ONNX_TEST_MODEL "" CACHE FILEPATH
    "Optional absolute ONNX model used by the backend-boundary metadata test")
set(XVISION_ONNX_EXECUTION_TEST_MODEL "" CACHE FILEPATH
    "Optional absolute one-input/one-output identity ONNX model used by the execution test")
add_executable(XvVisionBackendBoundaryTests
    tst_backendboundaries.cpp
    ../XvFuncCollection/XvFuncSystem/Interop/OnnxSession.cpp
    ../XvFuncCollection/XvFuncSystem/Interop/XvOpenCvImageInterop.cpp
)
set_target_properties(XvVisionBackendBoundaryTests PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)
target_include_directories(XvVisionBackendBoundaryTests PRIVATE
    ../XvFuncCollection/XvFuncSystem/include
    ../XvFuncCollection/XvFuncSystem/Interop
)
target_compile_definitions(XvVisionBackendBoundaryTests PRIVATE
    XVFUNCSYSTEM_LIBRARY
)
target_link_libraries(XvVisionBackendBoundaryTests PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Gui
    Qt${QT_VERSION_MAJOR}::Test
)
if(XVISION_ENABLE_OPENCV)
    target_compile_definitions(XvVisionBackendBoundaryTests PRIVATE
        XVISION_ENABLE_OPENCV=1)
    target_link_libraries(XvVisionBackendBoundaryTests PRIVATE XVision::OpenCV)
endif()
if(XVISION_ENABLE_ONNXRUNTIME)
    target_compile_definitions(XvVisionBackendBoundaryTests PRIVATE
        XVISION_ENABLE_ONNXRUNTIME=1)
    target_link_libraries(XvVisionBackendBoundaryTests PRIVATE XVision::ONNXRuntime)
endif()
if(WIN32)
    foreach(_runtime_dll IN LISTS
            XVISION_OPENCV_RUNTIME_DLLS XVISION_ONNXRUNTIME_RUNTIME_DLLS)
        if(_runtime_dll)
            add_custom_command(TARGET XvVisionBackendBoundaryTests POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${_runtime_dll}"
                    $<TARGET_FILE_DIR:XvVisionBackendBoundaryTests>
            )
        endif()
    endforeach()
endif()
add_test(NAME visionmaster_backend_boundary
    COMMAND ${CMAKE_COMMAND} -E env
        "QT_QPA_PLATFORM=offscreen"
        "XVISION_ONNX_TEST_MODEL=${XVISION_ONNX_TEST_MODEL}"
        "XVISION_ONNX_EXECUTION_TEST_MODEL=${XVISION_ONNX_EXECUTION_TEST_MODEL}"
        "PATH=$<TARGET_FILE_DIR:XvVisionBackendBoundaryTests>$<SEMICOLON>$ENV{QTDIR}/bin$<SEMICOLON>$ENV{PATH}"
        $<TARGET_FILE:XvVisionBackendBoundaryTests>
        -o "${XVISION_QT_TEST_LOG_DIR}/visionmaster_backend_boundary.txt,txt"
        -o "-,txt"
)

add_executable(XvOpenCvImageOperatorTests
    tst_opencvimageoperators.cpp
)
set_target_properties(XvOpenCvImageOperatorTests PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)
target_include_directories(XvOpenCvImageOperatorTests PRIVATE
    ../XvCore/include
    ../XvData/include
    ../XvFuncCollection/XvFuncSystem/include
    ../XvFuncCollection/XvFuncSystem/ImageProcessing
)
target_link_libraries(XvOpenCvImageOperatorTests PRIVATE
    XvCore
    XvData
    XvFuncSystem
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Gui
    Qt${QT_VERSION_MAJOR}::Test
    Qt${QT_VERSION_MAJOR}::Widgets
)
if(XVISION_ENABLE_OPENCV)
    target_compile_definitions(XvOpenCvImageOperatorTests PRIVATE
        XVISION_ENABLE_OPENCV=1)
    target_link_libraries(XvOpenCvImageOperatorTests PRIVATE XVision::OpenCV)
endif()
add_test(NAME opencv_image_operators
    COMMAND ${CMAKE_COMMAND} -E env
        "QT_QPA_PLATFORM=offscreen"
        "PATH=$<TARGET_FILE_DIR:XVision>$<SEMICOLON>$<TARGET_FILE_DIR:XvFuncSystem>$<SEMICOLON>$<TARGET_FILE_DIR:XvCore>$<SEMICOLON>$ENV{QTDIR}/bin$<SEMICOLON>$ENV{PATH}"
        $<TARGET_FILE:XvOpenCvImageOperatorTests>
        -o "${XVISION_QT_TEST_LOG_DIR}/opencv_image_operators.txt,txt"
        -o "-,txt"
)

add_executable(XvOpenCvMorphologyDetectorTests
    tst_opencvmorphologydetectors.cpp
)
set_target_properties(XvOpenCvMorphologyDetectorTests PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)
target_include_directories(XvOpenCvMorphologyDetectorTests PRIVATE
    ../XvCore/include
    ../XvData/include
    ../XvFuncCollection/XvFuncSystem/include
    ../XvFuncCollection/XvFuncSystem/ImageProcessing
    ../XvFuncCollection/XvFuncSystem/MorphologyDetection
)
target_link_libraries(XvOpenCvMorphologyDetectorTests PRIVATE
    XvCore
    XvData
    XvFuncSystem
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Gui
    Qt${QT_VERSION_MAJOR}::Test
    Qt${QT_VERSION_MAJOR}::Widgets
)
if(XVISION_ENABLE_OPENCV)
    target_compile_definitions(XvOpenCvMorphologyDetectorTests PRIVATE
        XVISION_ENABLE_OPENCV=1)
    target_link_libraries(XvOpenCvMorphologyDetectorTests PRIVATE XVision::OpenCV)
endif()
add_test(NAME opencv_morphology_detectors
    COMMAND ${CMAKE_COMMAND} -E env
        "QT_QPA_PLATFORM=offscreen"
        "PATH=$<TARGET_FILE_DIR:XVision>$<SEMICOLON>$<TARGET_FILE_DIR:XvFuncSystem>$<SEMICOLON>$<TARGET_FILE_DIR:XvCore>$<SEMICOLON>$ENV{QTDIR}/bin$<SEMICOLON>$ENV{PATH}"
        $<TARGET_FILE:XvOpenCvMorphologyDetectorTests>
        -o "${XVISION_QT_TEST_LOG_DIR}/opencv_morphology_detectors.txt,txt"
        -o "-,txt"
)

add_executable(XvOpenCvTemplatingTests
    tst_opencvtemplating.cpp
)
set_target_properties(XvOpenCvTemplatingTests PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)
target_include_directories(XvOpenCvTemplatingTests PRIVATE
    ../XvCore/include
    ../XvData/include
    ../XvFuncCollection/XvFuncSystem/include
    ../XvFuncCollection/XvFuncSystem/ImageProcessing
    ../XvFuncCollection/XvFuncSystem/TemplateRectification
)
target_link_libraries(XvOpenCvTemplatingTests PRIVATE
    XvCore
    XvData
    XvFuncSystem
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Gui
    Qt${QT_VERSION_MAJOR}::Test
    Qt${QT_VERSION_MAJOR}::Widgets
)
if(XVISION_ENABLE_OPENCV)
    target_compile_definitions(XvOpenCvTemplatingTests PRIVATE
        XVISION_ENABLE_OPENCV=1)
    target_link_libraries(XvOpenCvTemplatingTests PRIVATE XVision::OpenCV)
endif()
add_test(NAME opencv_template_rectification
    COMMAND ${CMAKE_COMMAND} -E env
        "QT_QPA_PLATFORM=offscreen"
        "PATH=$<TARGET_FILE_DIR:XVision>$<SEMICOLON>$<TARGET_FILE_DIR:XvFuncSystem>$<SEMICOLON>$<TARGET_FILE_DIR:XvCore>$<SEMICOLON>$ENV{QTDIR}/bin$<SEMICOLON>$ENV{PATH}"
        $<TARGET_FILE:XvOpenCvTemplatingTests>
        -o "${XVISION_QT_TEST_LOG_DIR}/opencv_template_rectification.txt,txt"
        -o "-,txt"
)

add_executable(XvGeometryMeasurementTests
    tst_geometrymeasurement.cpp
)
set_target_properties(XvGeometryMeasurementTests PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)
target_include_directories(XvGeometryMeasurementTests PRIVATE
    ../XvCore/include
    ../XvData/include
    ../XvFuncCollection/XvFuncSystem/include
    ../XvFuncCollection/XvFuncSystem/Measurement
)
target_link_libraries(XvGeometryMeasurementTests PRIVATE
    XvCore
    XvData
    XvFuncSystem
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Gui
    Qt${QT_VERSION_MAJOR}::Test
    Qt${QT_VERSION_MAJOR}::Widgets
)
add_test(NAME geometry_measurement
    COMMAND ${CMAKE_COMMAND} -E env
        "QT_QPA_PLATFORM=offscreen"
        "PATH=$<TARGET_FILE_DIR:XVision>$<SEMICOLON>$<TARGET_FILE_DIR:XvFuncSystem>$<SEMICOLON>$<TARGET_FILE_DIR:XvCore>$<SEMICOLON>$ENV{QTDIR}/bin$<SEMICOLON>$ENV{PATH}"
        $<TARGET_FILE:XvGeometryMeasurementTests>
        -o "${XVISION_QT_TEST_LOG_DIR}/geometry_measurement.txt,txt"
        -o "-,txt"
)

add_executable(XvDetectRecordTests
    tst_detectrecord.cpp
)
set_target_properties(XvDetectRecordTests PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)
target_include_directories(XvDetectRecordTests PRIVATE
    ../XvCore/include
    ../XvData/include
    ../XvFuncCollection/XvFuncSystem/include
    ../XvFuncCollection/XvFuncSystem/Record
)
target_link_libraries(XvDetectRecordTests PRIVATE
    XvCore
    XvData
    XvFuncSystem
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Gui
    Qt${QT_VERSION_MAJOR}::Sql
    Qt${QT_VERSION_MAJOR}::Test
    Qt${QT_VERSION_MAJOR}::Widgets
)
add_test(NAME detect_record
    COMMAND ${CMAKE_COMMAND} -E env
        "QT_QPA_PLATFORM=offscreen"
        "PATH=$<TARGET_FILE_DIR:XVision>$<SEMICOLON>$<TARGET_FILE_DIR:XvFuncSystem>$<SEMICOLON>$<TARGET_FILE_DIR:XvCore>$<SEMICOLON>$ENV{QTDIR}/bin$<SEMICOLON>$ENV{PATH}"
        $<TARGET_FILE:XvDetectRecordTests>
        -o "${XVISION_QT_TEST_LOG_DIR}/detect_record.txt,txt"
        -o "-,txt"
)

add_executable(XvCommunicationOperatorTests
    tst_communicationoperators.cpp
)
set_target_properties(XvCommunicationOperatorTests PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)
target_include_directories(XvCommunicationOperatorTests PRIVATE
    ../XvCore/include
    ../XvData/include
    ../XvFuncCollection/XvFuncSystem/include
    ../XvFuncCollection/XvFuncSystem/Communication
)
target_link_libraries(XvCommunicationOperatorTests PRIVATE
    XvCore
    XvData
    XvFuncSystem
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Gui
    Qt${QT_VERSION_MAJOR}::Network
    Qt${QT_VERSION_MAJOR}::SerialBus
    Qt${QT_VERSION_MAJOR}::SerialPort
    Qt${QT_VERSION_MAJOR}::Test
    Qt${QT_VERSION_MAJOR}::Widgets
)
add_test(NAME communication_operators
    COMMAND ${CMAKE_COMMAND} -E env
        "QT_QPA_PLATFORM=offscreen"
        "PATH=$<TARGET_FILE_DIR:XVision>$<SEMICOLON>$<TARGET_FILE_DIR:XvFuncSystem>$<SEMICOLON>$<TARGET_FILE_DIR:XvCore>$<SEMICOLON>$ENV{QTDIR}/bin$<SEMICOLON>$ENV{PATH}"
        $<TARGET_FILE:XvCommunicationOperatorTests>
        -o "${XVISION_QT_TEST_LOG_DIR}/communication_operators.txt,txt"
        -o "-,txt"
)

add_executable(XvSourceNotificationTests
    tst_sourcenotification.cpp
)
set_target_properties(XvSourceNotificationTests PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)
target_include_directories(XvSourceNotificationTests PRIVATE
    ../XvCore/include
    ../XvData/include
    ../XvFuncCollection/XvFuncSystem/include
    ../XvFuncCollection/XvFuncSystem/ImageAcquisition
    ../XvFuncCollection/XvFuncSystem/Other
    ../CommonUsing/Project/XLog/include
)
target_link_libraries(XvSourceNotificationTests PRIVATE
    XvCore
    XvData
    XvFuncSystem
    XLog
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Gui
    Qt${QT_VERSION_MAJOR}::Test
    Qt${QT_VERSION_MAJOR}::Widgets
)
if(XVISION_ENABLE_OPENCV)
    target_compile_definitions(XvSourceNotificationTests PRIVATE
        XVISION_ENABLE_OPENCV=1)
    target_link_libraries(XvSourceNotificationTests PRIVATE XVision::OpenCV)
endif()
add_test(NAME source_notification
    COMMAND ${CMAKE_COMMAND} -E env
        "QT_QPA_PLATFORM=offscreen"
        "PATH=$<TARGET_FILE_DIR:XVision>$<SEMICOLON>$<TARGET_FILE_DIR:XvFuncSystem>$<SEMICOLON>$<TARGET_FILE_DIR:XvCore>$<SEMICOLON>$ENV{QTDIR}/bin$<SEMICOLON>$ENV{PATH}"
        $<TARGET_FILE:XvSourceNotificationTests>
        -o "${XVISION_QT_TEST_LOG_DIR}/source_notification.txt,txt"
        -o "-,txt"
)

add_executable(XvOnnxOperatorTests
    tst_onnxoperators.cpp
)
set_target_properties(XvOnnxOperatorTests PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)
target_include_directories(XvOnnxOperatorTests PRIVATE
    ../XvCore/include
    ../XvData/include
    ../XvFuncCollection/XvFuncSystem/include
    ../XvFuncCollection/XvFuncSystem/Interop
    ../XvFuncCollection/XvFuncSystem/Onnx
)
target_link_libraries(XvOnnxOperatorTests PRIVATE
    XvCore
    XvData
    XvFuncSystem
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Gui
    Qt${QT_VERSION_MAJOR}::Test
    Qt${QT_VERSION_MAJOR}::Widgets
)
if(XVISION_ENABLE_ONNXRUNTIME)
    target_compile_definitions(XvOnnxOperatorTests PRIVATE
        XVISION_ENABLE_ONNXRUNTIME=1)
    target_link_libraries(XvOnnxOperatorTests PRIVATE XVision::ONNXRuntime)
endif()
if(WIN32 AND XVISION_ENABLE_ONNXRUNTIME)
    foreach(_runtime_dll IN LISTS XVISION_ONNXRUNTIME_RUNTIME_DLLS)
        if(_runtime_dll)
            add_custom_command(TARGET XvOnnxOperatorTests POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${_runtime_dll}"
                    $<TARGET_FILE_DIR:XvOnnxOperatorTests>
            )
        endif()
    endforeach()
endif()
add_test(NAME onnx_operators
    COMMAND ${CMAKE_COMMAND} -E env
        "QT_QPA_PLATFORM=offscreen"
        "PATH=$<TARGET_FILE_DIR:XVision>$<SEMICOLON>$<TARGET_FILE_DIR:XvFuncSystem>$<SEMICOLON>$<TARGET_FILE_DIR:XvCore>$<SEMICOLON>$ENV{QTDIR}/bin$<SEMICOLON>$ENV{PATH}"
        $<TARGET_FILE:XvOnnxOperatorTests>
        -o "${XVISION_QT_TEST_LOG_DIR}/onnx_operators.txt,txt"
        -o "-,txt"
)

add_executable(XvCorePersistenceTests
    tst_projectxml.cpp
)
set_target_properties(XvCorePersistenceTests PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

target_include_directories(XvCorePersistenceTests PRIVATE
    ../XvCore/include
    ../XvData/include
    ../XvDisplay/include
    ../XvCamera/include
    ../XvTokenMsg/include
    ../XvFuncCollection/XvFuncSystem/include
    ../XvFuncCollection/XvFuncSystem/DataProcessing
    ../XvFuncCollection/XvFuncSystem/ImageAcquisition
    ../XvFuncCollection/XvFuncSystem/Location
    ../XvFuncCollection/XvFuncSystem/Detection
    ../XvFuncCollection/XvFuncSystem/Segmentation
    ../XvFuncCollection/XvFuncSystem/ImageProcessing
    ../XvFuncCollection/XvFuncSystem/MorphologyDetection
    ../XvFuncCollection/XvFuncSystem/Logic
    ../XvFuncCollection/XvFuncSystem/Other
    ../XvFuncCollection/XvFuncSystem/Interop
    ../XvFuncCollection/XvFuncSystem/Onnx
    ../XvFuncCollection/XvFuncSystem/TemplateRectification
    ../XvFuncCollection/XvFuncSystem/Measurement
    ../XvFuncCollection/XvFuncSystem/Record
    ../XvFuncCollection/XvFuncSystem/Communication
    ../CommonUsing/Project/XLanguage/include
    ../CommonUsing/Project/XLog/include
)

target_link_libraries(XvCorePersistenceTests PRIVATE
    XvCore
    XvData
    XvDisplay
    XvCamera
    XvFuncSystem
    XvTokenMsg
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Test
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Xml
)


if(XVISION_ENABLE_OPENCV)
    target_compile_definitions(XvCorePersistenceTests PRIVATE XVISION_ENABLE_OPENCV=1)
endif()

if(XVISION_ENABLE_ONNXRUNTIME)
    target_compile_definitions(XvCorePersistenceTests PRIVATE
        XVISION_ENABLE_ONNXRUNTIME=1)
endif()

add_test(NAME projectxml
    COMMAND ${CMAKE_COMMAND} -E env
        "QT_QPA_PLATFORM=offscreen"
        "PATH=$<TARGET_FILE_DIR:XVision>$<SEMICOLON>$<TARGET_FILE_DIR:XvFuncSystem>$<SEMICOLON>$<TARGET_FILE_DIR:XvCamera>$<SEMICOLON>$ENV{QTDIR}/bin$<SEMICOLON>$ENV{PATH}"
        $<TARGET_FILE:XvCorePersistenceTests>
        -o "${XVISION_QT_TEST_LOG_DIR}/projectxml.txt,txt"
        -o "-,txt"
)

add_executable(XvCameraTests
    tst_camera.cpp
)
set_target_properties(XvCameraTests PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

target_include_directories(XvCameraTests PRIVATE
    ../XvCamera/include
)

target_link_libraries(XvCameraTests PRIVATE
    XvCamera
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Gui
    Qt${QT_VERSION_MAJOR}::Test
)

add_test(NAME camera
    COMMAND ${CMAKE_COMMAND} -E env
        "QT_QPA_PLATFORM=offscreen"
        "PATH=$<TARGET_FILE_DIR:XvCameraTests>$<SEMICOLON>$<TARGET_FILE_DIR:XVision>$<SEMICOLON>$ENV{QTDIR}/bin$<SEMICOLON>$ENV{PATH}"
        $<TARGET_FILE:XvCameraTests>
        -o "${XVISION_QT_TEST_LOG_DIR}/camera.txt,txt"
        -o "-,txt"
)

add_executable(XvSystemPluginTests
    tst_systemplugin.cpp
)
set_target_properties(XvSystemPluginTests PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

target_include_directories(XvSystemPluginTests PRIVATE
    ../XvCore/include
    ../XvData/include
    ../XvTokenMsg/include
)

target_link_libraries(XvSystemPluginTests PRIVATE
    XvCore
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Gui
    Qt${QT_VERSION_MAJOR}::Test
    Qt${QT_VERSION_MAJOR}::Xml
)
add_dependencies(XvSystemPluginTests XvFuncSystem)

add_test(NAME systemplugin
    COMMAND ${CMAKE_COMMAND} -E env
        "PATH=$<TARGET_FILE_DIR:XvFuncSystem>$<SEMICOLON>$<TARGET_FILE_DIR:XvCore>$<SEMICOLON>$ENV{QTDIR}/bin$<SEMICOLON>$ENV{PATH}"
        $<TARGET_FILE:XvSystemPluginTests>
        $<TARGET_FILE:XvFuncSystem>
        "${VISIONMASTER_MATRIX}"
        -o "${XVISION_QT_TEST_LOG_DIR}/systemplugin.txt,txt"
        -o "-,txt"
)

# The SDK is intentionally not installed at Qt's compiled-in prefix on CI.
# Resolve GUI platform plugins explicitly for build-tree tests; package tests
# separately use only the staged runtime and staged plugins.
if(WIN32)
    set_tests_properties(
        visionmaster_data
        visionmaster_backend_boundary
        opencv_image_operators
        opencv_morphology_detectors
        opencv_template_rectification
        geometry_measurement
        detect_record
        communication_operators
        source_notification
        onnx_operators
        projectxml
        camera
        systemplugin
        PROPERTIES ENVIRONMENT
            "QT_PLUGIN_PATH=$ENV{QTDIR}/plugins;QT_LOGGING_TO_CONSOLE=1"
    )
endif()

# Presentation resources and localization must also work in the deployed Qt runtime.
add_executable(XvUiAppearanceTests
    tst_uiappearance.cpp
    ../XVision/Utils/UiAppearance.cpp
    ../XVision/Res/AppRes.qrc
)
set_target_properties(XvUiAppearanceTests PROPERTIES
    CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON AUTORCC ON)
target_include_directories(XvUiAppearanceTests PRIVATE
    ../XVision/Utils ../CommonUsing/Project/XLanguage/include ../XvCore/include
    ../XvData/include)
target_link_libraries(XvUiAppearanceTests PRIVATE
    XvCore XLanguage Qt${QT_VERSION_MAJOR}::Test Qt${QT_VERSION_MAJOR}::Widgets)
add_test(NAME ui_appearance
    COMMAND ${CMAKE_COMMAND} -E env
        "QT_QPA_PLATFORM=offscreen"
        "PATH=$<TARGET_FILE_DIR:XVision>$<SEMICOLON>$ENV{QTDIR}/bin$<SEMICOLON>$ENV{PATH}"
        $<TARGET_FILE:XvUiAppearanceTests>
        -o "${XVISION_QT_TEST_LOG_DIR}/ui_appearance.txt,txt" -o "-,txt")

if(WIN32)
    set_tests_properties(ui_appearance PROPERTIES ENVIRONMENT
        "QT_PLUGIN_PATH=$ENV{QTDIR}/plugins;QT_LOGGING_TO_CONSOLE=1")
endif()

# Real application widgets are compiled into the UI regression executable.
add_executable(XvUiResponsivenessTests
    tst_uiresponsiveness.cpp
    ../XVision/UiWork/UiXvFunc/FrmXvFuncAsm.cpp
    ../XVision/UiWork/UiXvFunc/FrmXvFuncAsm.ui
    ../XVision/UiWork/UiXvFunc/FrmXvFuncType.cpp
    ../XVision/UiWork/UiXvFunc/FrmXvFuncType.ui
    ../XVision/UiWork/UiXvFunc/WdgItemXvFunc.cpp
    ../XVision/UiWork/UiXvFunc/WdgItemXvFunc.ui
    ../XVision/Res/AppRes.qrc
    ../XVision/Model/BaseWidget.cpp
    ../XVision/UIOutput/FrmLogShow.cpp
    ../XVision/UIOutput/FrmLogShow.ui
    ../XvFuncCollection/XvFuncSystem/Model/BaseSystemFuncWdg.cpp
)
set_target_properties(XvUiResponsivenessTests PROPERTIES
    CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON AUTOMOC ON AUTOUIC ON AUTORCC ON)
get_target_property(XVISION_UI_TEST_INCLUDES XVision INCLUDE_DIRECTORIES)
target_include_directories(XvUiResponsivenessTests PRIVATE
    ${XVISION_UI_TEST_INCLUDES}
    ../XvFuncCollection/XvFuncSystem/Model
    ../XvFuncCollection/XvFuncSystem/include
    ../XvFuncCollection/XvFuncSystem/Other)
target_link_libraries(XvUiResponsivenessTests PRIVATE
    XvFuncSystem XvCore XvData XvDisplay XWidget XLog XLanguage
    Qt${QT_VERSION_MAJOR}::Test Qt${QT_VERSION_MAJOR}::Widgets)

add_executable(XvOnnxCpuBenchmark bench_onnxcpu.cpp)
set_target_properties(XvOnnxCpuBenchmark PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
target_include_directories(XvOnnxCpuBenchmark PRIVATE
    ../XvFuncCollection/XvFuncSystem/Onnx
    ../XvFuncCollection/XvFuncSystem/Interop
    ../XvFuncCollection/XvFuncSystem/include)
target_link_libraries(XvOnnxCpuBenchmark PRIVATE XvFuncSystem XvCore XvData XLanguage
    Qt${QT_VERSION_MAJOR}::Core Qt${QT_VERSION_MAJOR}::Widgets)

find_package(Threads REQUIRED)
target_link_libraries(XvSourceNotificationTests PRIVATE Threads::Threads)
target_link_libraries(XvUiResponsivenessTests PRIVATE Threads::Threads)

target_compile_definitions(XvOnnxOperatorTests PRIVATE
    XVISION_ONNX_FIXTURE="${CMAKE_CURRENT_SOURCE_DIR}/fixtures/cpu-identity.onnx")
add_test(NAME ui_responsiveness
    COMMAND ${CMAKE_COMMAND} -E env "QT_QPA_PLATFORM=offscreen"
        "PATH=$<TARGET_FILE_DIR:XVision>$<SEMICOLON>$<TARGET_FILE_DIR:XvFuncSystem>$<SEMICOLON>$ENV{QTDIR}/bin$<SEMICOLON>$ENV{PATH}"
        $<TARGET_FILE:XvUiResponsivenessTests>
        -o "${XVISION_QT_TEST_LOG_DIR}/ui_responsiveness.txt,txt" -o "-,txt")
if(WIN32)
    set_tests_properties(ui_responsiveness PROPERTIES ENVIRONMENT
        "QT_PLUGIN_PATH=$ENV{QTDIR}/plugins;QT_LOGGING_TO_CONSOLE=1")
endif()

target_link_libraries(XvCommunicationOperatorTests PRIVATE Threads::Threads)
