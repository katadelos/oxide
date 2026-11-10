set(fbink_source "${PROJECT_SOURCE_DIR}/third_party/fbink")
set(fbink_build "${PROJECT_BINARY_DIR}/fbink")
set(fbink_patch "${fbink_build}/patch")
if(NOT EXISTS "${fbink_source}/fbink.c" OR
   NOT EXISTS "${fbink_source}/font8x8/font8x8_basic.h")
    message(FATAL_ERROR "Initialize dependencies with ./scripts/checkout-deps")
endif()
find_program(OXIDE_PATCH_EXECUTABLE patch)
if(NOT OXIDE_PATCH_EXECUTABLE)
    message(FATAL_ERROR "The patch utility is required to build FBInk")
endif()
# Patch build copies; fbink.c includes fbink_device_id.c directly.
configure_file("${fbink_source}/fbink.c" "${fbink_build}/fbink.c" COPYONLY)
configure_file("${fbink_source}/fbink_device_id.c" "${fbink_patch}/fbink_device_id.c" COPYONLY)
configure_file(third_party/fbink-pw7.patch "${fbink_patch}/pw7.patch" COPYONLY)
execute_process(
    COMMAND "${OXIDE_PATCH_EXECUTABLE}" --batch --forward -p1 -i pw7.patch
    WORKING_DIRECTORY "${fbink_patch}"
    RESULT_VARIABLE patch_result
    OUTPUT_VARIABLE patch_output
    ERROR_VARIABLE patch_error
)
if(NOT patch_result EQUAL 0)
    message(FATAL_ERROR "Cannot apply FBInk PW7 patch: ${patch_output}${patch_error}")
endif()
configure_file("${fbink_patch}/fbink_device_id.c" "${fbink_build}/fbink_device_id.c" COPYONLY)
add_library(fbink STATIC
    "${fbink_build}/fbink.c"
    third_party/fbink/cutef8/utf8.c
    third_party/fbink/cutef8/dfa.c
)
target_include_directories(fbink SYSTEM PUBLIC third_party/fbink)
target_compile_definitions(fbink PRIVATE
    FBINK_FOR_KINDLE FBINK_MINIMAL FBINK_WITH_DRAW FBINK_WITH_BITMAP
    FBINK_VERSION="886f25f-pw7"
)
set_target_properties(fbink PROPERTIES C_STANDARD 11 C_VISIBILITY_PRESET hidden)
target_link_libraries(oxide PRIVATE fbink)
