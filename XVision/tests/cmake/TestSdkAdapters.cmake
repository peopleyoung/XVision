cmake_minimum_required(VERSION 3.21)

foreach(_required XVISION_SOURCE_DIR FIXTURE_SOURCE_DIR TEST_BINARY_DIR)
    if(NOT DEFINED ${_required} OR "${${_required}}" STREQUAL "")
        message(FATAL_ERROR "${_required} is required")
    endif()
endforeach()

function(run_configure_case name should_succeed expected_error)
    set(_build_dir "${TEST_BINARY_DIR}/${name}")
    file(REMOVE_RECURSE "${_build_dir}")
    execute_process(
        COMMAND "${CMAKE_COMMAND}"
            -S "${FIXTURE_SOURCE_DIR}"
            -B "${_build_dir}"
            "-DXVISION_SOURCE_DIR=${XVISION_SOURCE_DIR}"
            ${ARGN}
        RESULT_VARIABLE _result
        OUTPUT_VARIABLE _stdout
        ERROR_VARIABLE _stderr
    )
    set(_output "${_stdout}\n${_stderr}")
    if(should_succeed)
        if(NOT _result EQUAL 0)
            message(FATAL_ERROR
                "SDK adapter case '${name}' should configure successfully:\n${_output}")
        endif()
    else()
        if(_result EQUAL 0)
            message(FATAL_ERROR
                "SDK adapter case '${name}' should fail configuration")
        endif()
        if(NOT "${_output}" MATCHES "${expected_error}")
            message(FATAL_ERROR
                "SDK adapter case '${name}' did not report '${expected_error}':\n${_output}")
        endif()
    endif()
endfunction()

file(REMOVE_RECURSE "${TEST_BINARY_DIR}")
file(MAKE_DIRECTORY "${TEST_BINARY_DIR}")

run_configure_case(disabled TRUE "")
run_configure_case(opencv_missing_root FALSE "XVISION_OPENCV_ROOT is empty"
    -DXVISION_ENABLE_OPENCV=ON)
run_configure_case(onnx_missing_root FALSE "XVISION_ONNXRUNTIME_ROOT is empty"
    -DXVISION_ENABLE_ONNXRUNTIME=ON)

set(_incomplete_root "${TEST_BINARY_DIR}/incomplete-sdk")
file(MAKE_DIRECTORY "${_incomplete_root}")
run_configure_case(opencv_incomplete FALSE "OpenCV SDK is incomplete"
    -DXVISION_ENABLE_OPENCV=ON
    "-DXVISION_OPENCV_ROOT=${_incomplete_root}")
run_configure_case(onnx_incomplete FALSE "ONNX Runtime SDK is incomplete"
    -DXVISION_ENABLE_ONNXRUNTIME=ON
    "-DXVISION_ONNXRUNTIME_ROOT=${_incomplete_root}")

set(_opencv_root "${TEST_BINARY_DIR}/opencv-sdk")
set(_onnx_root "${TEST_BINARY_DIR}/onnx-sdk")
file(MAKE_DIRECTORY
    "${_opencv_root}/include/opencv2/video" "${_opencv_root}/lib" "${_opencv_root}/bin"
    "${_onnx_root}/include" "${_onnx_root}/lib" "${_onnx_root}/bin")
file(WRITE "${_opencv_root}/include/opencv2/core.hpp" "// fixture\n")
file(WRITE "${_opencv_root}/include/opencv2/imgproc.hpp" "// fixture\n")
file(WRITE "${_opencv_root}/include/opencv2/features2d.hpp" "// fixture\n")
file(WRITE "${_opencv_root}/include/opencv2/xfeatures2d.hpp" "// fixture\n")
file(WRITE "${_opencv_root}/include/opencv2/calib3d.hpp" "// fixture\n")
file(WRITE "${_opencv_root}/include/opencv2/photo.hpp" "// fixture\n")
file(WRITE "${_opencv_root}/include/opencv2/objdetect.hpp" "// fixture\n")
file(WRITE "${_opencv_root}/include/opencv2/stitching.hpp" "// fixture\n")
file(WRITE "${_opencv_root}/include/opencv2/ml.hpp" "// fixture\n")
file(WRITE "${_opencv_root}/include/opencv2/dnn_superres.hpp" "// fixture\n")
file(WRITE "${_opencv_root}/include/opencv2/videoio.hpp" "// fixture\n")
file(WRITE "${_opencv_root}/include/opencv2/video/background_segm.hpp" "// fixture\n")
file(WRITE "${_onnx_root}/include/onnxruntime_cxx_api.h" "// fixture\n")
file(WRITE "${_onnx_root}/include/onnxruntime_c_api.h" "// fixture\n")

if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Windows")
    file(WRITE "${_opencv_root}/lib/opencv_world4100.lib" "fixture")
    file(WRITE "${_opencv_root}/bin/opencv_world4100.dll" "fixture")
    file(WRITE "${_onnx_root}/lib/onnxruntime.lib" "fixture")
    file(WRITE "${_onnx_root}/bin/onnxruntime.dll" "fixture")
elseif(CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin")
    file(WRITE "${_opencv_root}/lib/libopencv_world.dylib" "fixture")
    file(WRITE "${_onnx_root}/lib/libonnxruntime.dylib" "fixture")
else()
    file(WRITE "${_opencv_root}/lib/libopencv_world.so" "fixture")
    file(WRITE "${_onnx_root}/lib/libonnxruntime.so" "fixture")
endif()

run_configure_case(complete TRUE ""
    -DXVISION_ENABLE_OPENCV=ON
    "-DXVISION_OPENCV_ROOT=${_opencv_root}"
    -DXVISION_ENABLE_ONNXRUNTIME=ON
    "-DXVISION_ONNXRUNTIME_ROOT=${_onnx_root}")

# LANGUAGES NONE lets non-Windows hosts verify Windows imported-target contracts.
file(WRITE "${_opencv_root}/lib/opencv_world4100.lib" "fixture")
file(WRITE "${_opencv_root}/bin/opencv_world4100.dll" "fixture")
file(WRITE "${_opencv_root}/bin/opencv_videoio_ffmpeg4100_64.dll" "fixture")
file(WRITE "${_onnx_root}/lib/onnxruntime.lib" "fixture")
file(WRITE "${_onnx_root}/bin/onnxruntime.dll" "fixture")
run_configure_case(complete_windows TRUE ""
    -DCMAKE_SYSTEM_NAME=Windows
    -DEXPECT_VIDEO_RUNTIME_FILES=ON
    -DXVISION_ENABLE_OPENCV=ON
    "-DXVISION_OPENCV_ROOT=${_opencv_root}"
    -DXVISION_ENABLE_ONNXRUNTIME=ON
    "-DXVISION_ONNXRUNTIME_ROOT=${_onnx_root}")

message(STATUS
    "VisionMaster SDK adapters: disabled, missing-root, incomplete-root and complete-root cases passed")
