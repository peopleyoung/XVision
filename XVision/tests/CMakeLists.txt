find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Gui Network SerialBus SerialPort Sql Test Widgets Xml)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Gui Network SerialBus SerialPort Sql Test Widgets Xml)
include(../3rdparty/Common3rdparty.cmake)

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
        "PATH=$<TARGET_FILE_DIR:XvVisionMasterDataTests>$<SEMICOLON>$ENV{QTDIR}/bin$<SEMICOLON>$ENV{PATH}"
        $<TARGET_FILE:XvVisionMasterDataTests>
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
    ../3rdparty/halcon/include
    ../3rdparty/halcon/include/halconcpp
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
    ${HALCON_LIB}
)

target_link_directories(XvCorePersistenceTests PRIVATE
    ../${HALCON_LINK_DIR}
)

if(XVISION_ENABLE_ONNXRUNTIME)
    target_compile_definitions(XvCorePersistenceTests PRIVATE
        XVISION_ENABLE_ONNXRUNTIME=1)
endif()

add_test(NAME projectxml
    COMMAND ${CMAKE_COMMAND} -E env
        "QT_QPA_PLATFORM=offscreen"
        "PATH=$<TARGET_FILE_DIR:XVision>$<SEMICOLON>$<TARGET_FILE_DIR:XvFuncSystem>$<SEMICOLON>$<TARGET_FILE_DIR:XvCamera>$<SEMICOLON>$ENV{QTDIR}/bin$<SEMICOLON>$ENV{PATH}"
        $<TARGET_FILE:XvCorePersistenceTests>
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
        "PATH=$<TARGET_FILE_DIR:XvCameraTests>$<SEMICOLON>$ENV{QTDIR}/bin$<SEMICOLON>$ENV{PATH}"
        $<TARGET_FILE:XvCameraTests>
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
)
