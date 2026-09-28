cmake_minimum_required(VERSION 3.21)

foreach(REQUIRED_VARIABLE MATRIX VERIFIER TEST_BINARY_DIR)
    if(NOT DEFINED ${REQUIRED_VARIABLE} OR "${${REQUIRED_VARIABLE}}" STREQUAL "")
        message(FATAL_ERROR "${REQUIRED_VARIABLE} is required")
    endif()
endforeach()

file(READ "${MATRIX}" VALID_MATRIX)
file(MAKE_DIRECTORY "${TEST_BINARY_DIR}")

execute_process(
    COMMAND "${CMAKE_COMMAND}" "-DMATRIX=${MATRIX}" -DREQUIRE_CODE_COMPLETE=ON
        -P "${VERIFIER}"
    RESULT_VARIABLE VALID_RESULT
    OUTPUT_VARIABLE VALID_OUTPUT
    ERROR_VARIABLE VALID_ERROR_OUTPUT
)
if(NOT VALID_RESULT EQUAL 0)
    message(FATAL_ERROR
        "Code-complete baseline failed validation:\n${VALID_OUTPUT}\n${VALID_ERROR_OUTPUT}")
endif()

function(expect_matrix_failure NAME CONTENT EXPECTED_PATTERN)
    if(CONTENT STREQUAL VALID_MATRIX)
        message(FATAL_ERROR
            "Negative case '${NAME}' did not mutate the current baseline")
    endif()
    set(BAD_MATRIX "${TEST_BINARY_DIR}/${NAME}.csv")
    file(WRITE "${BAD_MATRIX}" "${CONTENT}")
    execute_process(
        COMMAND "${CMAKE_COMMAND}" "-DMATRIX=${BAD_MATRIX}"
            -DREQUIRE_CODE_COMPLETE=ON -P "${VERIFIER}"
        RESULT_VARIABLE RESULT
        OUTPUT_VARIABLE OUTPUT
        ERROR_VARIABLE ERROR_OUTPUT
    )
    if(RESULT EQUAL 0)
        message(FATAL_ERROR "Negative case '${NAME}' unexpectedly passed")
    endif()
    set(COMBINED_OUTPUT "${OUTPUT}\n${ERROR_OUTPUT}")
    if(NOT COMBINED_OUTPUT MATCHES "${EXPECTED_PATTERN}")
        message(FATAL_ERROR
            "Negative case '${NAME}' did not report '${EXPECTED_PATTERN}':\n${COMBINED_OUTPUT}")
    endif()
endfunction()

string(REPLACE
    "AddSutract,image,OImageArithmetic,add_subtract,mode,implemented_external_validation_pending"
    "AddSutract,image,OImageArithmetic,add_subtract,mode"
    MISSING_FIELD_MATRIX "${VALID_MATRIX}")
expect_matrix_failure(missing_field "${MISSING_FIELD_MATRIX}" "Line 2: expected 6 fields")

string(REPLACE
    "BitwiseNot,image,OImageArithmetic,bitwise_not,mode,implemented_external_validation_pending"
    "AddSutract,image,OImageArithmetic,bitwise_not,mode,implemented_external_validation_pending"
    DUPLICATE_NODE_MATRIX "${VALID_MATRIX}")
expect_matrix_failure(duplicate_node "${DUPLICATE_NODE_MATRIX}" "duplicate vm_node 'AddSutract'")

string(REPLACE
    "AddSutract,image,OImageArithmetic,add_subtract,mode,implemented_external_validation_pending"
    "AddSutract,image,MissingRole,add_subtract,mode,implemented_external_validation_pending"
    UNKNOWN_ROLE_MATRIX "${VALID_MATRIX}")
expect_matrix_failure(unknown_role "${UNKNOWN_ROLE_MATRIX}" "unknown xvision_role 'MissingRole'")

string(REPLACE
    "AddSutract,image,OImageArithmetic,add_subtract,mode,implemented_external_validation_pending"
    "AddSutract,image,OImageArithmetic,Add-Subtract,mode,implemented_external_validation_pending"
    INVALID_MODE_MATRIX "${VALID_MATRIX}")
expect_matrix_failure(invalid_mode "${INVALID_MODE_MATRIX}" "invalid mode 'Add-Subtract'")

string(REPLACE
    "AddSutract,image,OImageArithmetic,add_subtract,mode,implemented_external_validation_pending"
    "AddSutract,image,OImageArithmetic,add_subtract,unknown,implemented_external_validation_pending"
    UNKNOWN_MAPPING_MATRIX "${VALID_MATRIX}")
expect_matrix_failure(unknown_mapping "${UNKNOWN_MAPPING_MATRIX}" "unknown mapping 'unknown'")

string(REPLACE
    "AddSutract,image,OImageArithmetic,add_subtract,mode,implemented_external_validation_pending"
    "AddSutract,image,OImageArithmetic,add_subtract,mode,unknown"
    UNKNOWN_STATUS_MATRIX "${VALID_MATRIX}")
expect_matrix_failure(unknown_status "${UNKNOWN_STATUS_MATRIX}" "unknown status 'unknown'")

string(REPLACE
    "AddSutract,image,OImageArithmetic,add_subtract,mode,implemented_external_validation_pending"
    "AddSutract,image,OImageArithmetic,add_subtract,mode,planned"
    DEVELOPMENT_STATUS_MATRIX "${VALID_MATRIX}")
expect_matrix_failure(development_status "${DEVELOPMENT_STATUS_MATRIX}"
    "Code-complete matrix cannot contain planned/partial rows")

message(STATUS "VisionMaster matrix negative validation cases passed")
