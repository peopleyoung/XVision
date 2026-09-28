#include_dir:

set(AdsDocking_Inc CommonUsing/Project/AdsDocking/include)
set(XFlowGraphics_Inc CommonUsing/Project/XFlowGraphics/include)
set(XLanguage_Inc CommonUsing/Project/XLanguage/include)
set(XLog_Inc CommonUsing/Project/XLog/include)
set(XWidget_Inc CommonUsing/Project/XWidget/XWidget/include)
set(XConcurrent_Inc CommonUsing/Project/XConcurrent/include)


#link_dir:

if (NOT MSVC)
    set(OUTPUT_DIR mingw)
else()
    set(OUTPUT_DIR msvc)
endif()


# Multi-config generators resolve the dependency directory per configuration.
# This also works for single-config generators through the CONFIG expression.
set(COMMON_USING_LINK_DIR
    "CommonUsing/Lib$<$<CONFIG:Debug>:D>/${OUTPUT_DIR}")
