cmake_minimum_required(VERSION 3.21)

if(NOT DEFINED MATRIX OR MATRIX STREQUAL "")
    message(FATAL_ERROR "MATRIX must name the VisionMaster compatibility CSV")
endif()
if(NOT EXISTS "${MATRIX}")
    message(FATAL_ERROR "Matrix does not exist: ${MATRIX}")
endif()

set(EXPECTED_HEADER "vm_node,domain,xvision_role,mode,mapping,status")
set(EXPECTED_NODE_COUNT 126)
set(EXPECTED_ROLE_COUNT 31)

set(KNOWN_DOMAINS
    image morphology detection feature source output flow measurement onnx
    record communication template rectification
)
set(KNOWN_ROLES
    OImageArithmetic OImageFilter OImageAnalysis OImageColor OImageModel
    OImageTransform OImageThreshold OImageComposition OMorphology
    ORegionDetector OCodeDetector OPointFeature OCascadeDetector
    ImageAcquisition NotificationOutput ConditionalFlow LoopFlow
    GeometryCreate GeometryMeasure NObjectDetection NClassification
    NInference NSemanticSegmentation DetectRecord HttpJson ModbusRegister
    TcpText UdpText SerialData OTemplateMatch ORectification
)
set(KNOWN_MAPPINGS role mode alias preset)
set(KNOWN_STATUSES
    planned partial covered implemented validated
    implemented_external_validation_pending
)

file(STRINGS "${MATRIX}" MATRIX_LINES ENCODING UTF-8)
list(LENGTH MATRIX_LINES MATRIX_LINE_COUNT)
if(MATRIX_LINE_COUNT LESS 2)
    message(FATAL_ERROR "Matrix must contain a header and at least one node")
endif()
list(GET MATRIX_LINES 0 ACTUAL_HEADER)
if(NOT ACTUAL_HEADER STREQUAL EXPECTED_HEADER)
    message(FATAL_ERROR
        "Line 1: expected header '${EXPECTED_HEADER}', got '${ACTUAL_HEADER}'")
endif()

foreach(DOMAIN IN LISTS KNOWN_DOMAINS)
    set(DOMAIN_COUNT_${DOMAIN} 0)
endforeach()
foreach(ROLE IN LISTS KNOWN_ROLES)
    set(ROLE_COUNT_${ROLE} 0)
endforeach()
foreach(STATUS IN LISTS KNOWN_STATUSES)
    set(STATUS_COUNT_${STATUS} 0)
endforeach()

set(SEEN_NODES)
set(SEEN_ROLE_MODES)
set(NODE_COUNT 0)
set(ROWS)

math(EXPR LAST_LINE_INDEX "${MATRIX_LINE_COUNT} - 1")
foreach(LINE_INDEX RANGE 1 ${LAST_LINE_INDEX})
    list(GET MATRIX_LINES ${LINE_INDEX} LINE)
    math(EXPR LINE_NUMBER "${LINE_INDEX} + 1")
    if(LINE STREQUAL "")
        message(FATAL_ERROR "Line ${LINE_NUMBER}: blank rows are not allowed")
    endif()

    string(REPLACE "," ";" FIELDS "${LINE}")
    list(LENGTH FIELDS FIELD_COUNT)
    if(NOT FIELD_COUNT EQUAL 6)
        message(FATAL_ERROR
            "Line ${LINE_NUMBER}: expected 6 fields, got ${FIELD_COUNT}: '${LINE}'")
    endif()

    list(GET FIELDS 0 VM_NODE)
    list(GET FIELDS 1 DOMAIN)
    list(GET FIELDS 2 XVISION_ROLE)
    list(GET FIELDS 3 MODE)
    list(GET FIELDS 4 MAPPING)
    list(GET FIELDS 5 STATUS)

    if(NOT VM_NODE MATCHES "^[A-Za-z_][A-Za-z0-9_]*$")
        message(FATAL_ERROR
            "Line ${LINE_NUMBER}: invalid vm_node '${VM_NODE}'")
    endif()
    list(FIND SEEN_NODES "${VM_NODE}" NODE_INDEX)
    if(NOT NODE_INDEX EQUAL -1)
        message(FATAL_ERROR
            "Line ${LINE_NUMBER}: duplicate vm_node '${VM_NODE}'")
    endif()
    list(APPEND SEEN_NODES "${VM_NODE}")

    list(FIND KNOWN_DOMAINS "${DOMAIN}" DOMAIN_INDEX)
    if(DOMAIN_INDEX EQUAL -1)
        message(FATAL_ERROR
            "Line ${LINE_NUMBER}: unknown domain '${DOMAIN}'")
    endif()
    list(FIND KNOWN_ROLES "${XVISION_ROLE}" ROLE_INDEX)
    if(ROLE_INDEX EQUAL -1)
        message(FATAL_ERROR
            "Line ${LINE_NUMBER}: unknown xvision_role '${XVISION_ROLE}'")
    endif()
    if(NOT MODE MATCHES "^[a-z][a-z0-9_]*$")
        message(FATAL_ERROR
            "Line ${LINE_NUMBER}: invalid mode '${MODE}'")
    endif()
    list(FIND KNOWN_MAPPINGS "${MAPPING}" MAPPING_INDEX)
    if(MAPPING_INDEX EQUAL -1)
        message(FATAL_ERROR
            "Line ${LINE_NUMBER}: unknown mapping '${MAPPING}'")
    endif()
    list(FIND KNOWN_STATUSES "${STATUS}" STATUS_INDEX)
    if(STATUS_INDEX EQUAL -1)
        message(FATAL_ERROR
            "Line ${LINE_NUMBER}: unknown status '${STATUS}'")
    endif()

    set(ROLE_MODE "${XVISION_ROLE}:${MODE}")
    list(FIND SEEN_ROLE_MODES "${ROLE_MODE}" ROLE_MODE_INDEX)
    if(NOT ROLE_MODE_INDEX EQUAL -1)
        message(FATAL_ERROR
            "Line ${LINE_NUMBER}: duplicate role/mode '${ROLE_MODE}'")
    endif()
    list(APPEND SEEN_ROLE_MODES "${ROLE_MODE}")

    math(EXPR NODE_COUNT "${NODE_COUNT} + 1")
    math(EXPR DOMAIN_COUNT_${DOMAIN} "${DOMAIN_COUNT_${DOMAIN}} + 1")
    math(EXPR ROLE_COUNT_${XVISION_ROLE} "${ROLE_COUNT_${XVISION_ROLE}} + 1")
    math(EXPR STATUS_COUNT_${STATUS} "${STATUS_COUNT_${STATUS}} + 1")
    list(APPEND ROWS
        "| `${VM_NODE}` | `${DOMAIN}` | `${XVISION_ROLE}` | `${MODE}` | `${MAPPING}` | `${STATUS}` |")
endforeach()

if(NOT NODE_COUNT EQUAL EXPECTED_NODE_COUNT)
    message(FATAL_ERROR
        "Expected ${EXPECTED_NODE_COUNT} nodes, got ${NODE_COUNT}")
endif()

set(USED_ROLE_COUNT 0)
foreach(ROLE IN LISTS KNOWN_ROLES)
    if(ROLE_COUNT_${ROLE} EQUAL 0)
        message(FATAL_ERROR "Canonical role '${ROLE}' has no mapped nodes")
    endif()
    math(EXPR USED_ROLE_COUNT "${USED_ROLE_COUNT} + 1")
endforeach()
if(NOT USED_ROLE_COUNT EQUAL EXPECTED_ROLE_COUNT)
    message(FATAL_ERROR
        "Expected ${EXPECTED_ROLE_COUNT} roles, got ${USED_ROLE_COUNT}")
endif()

if(REQUIRE_CODE_COMPLETE)
    if(STATUS_COUNT_planned GREATER 0 OR STATUS_COUNT_partial GREATER 0)
        message(FATAL_ERROR
            "Code-complete matrix cannot contain planned/partial rows: "
            "planned=${STATUS_COUNT_planned}, partial=${STATUS_COUNT_partial}")
    endif()
endif()

if(DEFINED OUTPUT AND NOT OUTPUT STREQUAL "")
    get_filename_component(OUTPUT_DIRECTORY "${OUTPUT}" DIRECTORY)
    file(MAKE_DIRECTORY "${OUTPUT_DIRECTORY}")

    set(DOCUMENT "# VisionMaster 4.0 算子兼容清单\n\n")
    string(APPEND DOCUMENT
        "> 此文件由 `XVision/scripts/VerifyVisionMasterMatrix.cmake` 根据 "
        "`VisionMaster算子兼容矩阵.csv` 生成，请勿手工修改。\n\n")
    string(APPEND DOCUMENT "## 基线统计\n\n")
    string(APPEND DOCUMENT "- 上游节点：${NODE_COUNT}\n")
    string(APPEND DOCUMENT "- XVision 规范角色：${USED_ROLE_COUNT}\n")
    string(APPEND DOCUMENT "- 重复节点：0\n")
    string(APPEND DOCUMENT "- 未解析映射：0\n")
    foreach(STATUS IN LISTS KNOWN_STATUSES)
        if(STATUS_COUNT_${STATUS} GREATER 0)
            string(APPEND DOCUMENT
                "- 状态 `${STATUS}`：${STATUS_COUNT_${STATUS}}\n")
        endif()
    endforeach()

    string(APPEND DOCUMENT "\n## 能力域\n\n")
    string(APPEND DOCUMENT "| 能力域 | 节点数 |\n| --- | ---: |\n")
    foreach(DOMAIN IN LISTS KNOWN_DOMAINS)
        string(APPEND DOCUMENT "| `${DOMAIN}` | ${DOMAIN_COUNT_${DOMAIN}} |\n")
    endforeach()

    string(APPEND DOCUMENT "\n## 规范角色\n\n")
    string(APPEND DOCUMENT "| XVision 角色 | 映射节点数 |\n| --- | ---: |\n")
    foreach(ROLE IN LISTS KNOWN_ROLES)
        string(APPEND DOCUMENT "| `${ROLE}` | ${ROLE_COUNT_${ROLE}} |\n")
    endforeach()

    string(APPEND DOCUMENT "\n## 完整映射\n\n")
    string(APPEND DOCUMENT
        "| VisionMaster 节点 | 能力域 | XVision 角色 | 模式 | 映射方式 | 状态 |\n"
        "| --- | --- | --- | --- | --- | --- |\n")
    foreach(ROW IN LISTS ROWS)
        string(APPEND DOCUMENT "${ROW}\n")
    endforeach()
    file(WRITE "${OUTPUT}" "${DOCUMENT}")
endif()

if(DEFINED EXPECTED_DOCUMENT AND NOT EXPECTED_DOCUMENT STREQUAL "")
    if(NOT DEFINED OUTPUT OR OUTPUT STREQUAL "")
        message(FATAL_ERROR "EXPECTED_DOCUMENT requires OUTPUT")
    endif()
    if(NOT EXISTS "${EXPECTED_DOCUMENT}")
        message(FATAL_ERROR "Expected generated document is missing: ${EXPECTED_DOCUMENT}")
    endif()
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E compare_files
            "${OUTPUT}" "${EXPECTED_DOCUMENT}"
        RESULT_VARIABLE COMPARE_RESULT
    )
    if(NOT COMPARE_RESULT EQUAL 0)
        message(FATAL_ERROR
            "Generated document is stale: ${EXPECTED_DOCUMENT}. "
            "Regenerate it with VerifyVisionMasterMatrix.cmake and OUTPUT set to that path.")
    endif()
endif()

message(STATUS
    "VisionMaster matrix valid: ${NODE_COUNT} nodes / ${USED_ROLE_COUNT} roles / "
    "0 duplicates / 0 unresolved")
