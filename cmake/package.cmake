set(package_stage "${PROJECT_BINARY_DIR}/package/oxide")
set(package_archive "${PROJECT_SOURCE_DIR}/dist/oxide-${OXIDE_ARCH}.tar.gz")
file(GLOB package_examples CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/examples/*.sh")
set(package_dependencies
    oxide README.md LICENSE ${package_examples}
    third_party/fbink/LICENSE third_party/fbink/CREDITS third_party/fbink/font8x8/README
)
if(OXIDE_ARCH STREQUAL "kindle4")
    list(APPEND package_dependencies
        "${booklet_jar}" "${booklet_launcher_jar}" scripts/install-kindle4)
    set(package_commands
        COMMAND "${CMAKE_COMMAND}" -E copy
            "${booklet_jar}" "${booklet_launcher_jar}" "${package_stage}"
        COMMAND "${CMAKE_COMMAND}" -E copy
            "${PROJECT_SOURCE_DIR}/scripts/install-kindle4" "${package_stage}/install.sh"
    )
else()
    list(APPEND package_dependencies su third_party/openlipc/LICENSE.txt third_party/cjson/LICENSE)
    set(package_commands
        COMMAND "${CMAKE_COMMAND}" -E copy "$<TARGET_FILE:su>" "${package_stage}"
        COMMAND "${CMAKE_COMMAND}" -E copy
            "${PROJECT_SOURCE_DIR}/third_party/openlipc/LICENSE.txt" "${package_stage}/licenses/openlipc.txt"
        COMMAND "${CMAKE_COMMAND}" -E copy
            "${PROJECT_SOURCE_DIR}/third_party/cjson/LICENSE" "${package_stage}/licenses/cjson.txt"
    )
endif()
add_custom_command(
    OUTPUT "${package_archive}"
    COMMAND "${CMAKE_COMMAND}" -E rm -rf "${package_stage}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory
        "${package_stage}/examples" "${package_stage}/licenses" "${PROJECT_SOURCE_DIR}/dist"
    COMMAND "${CMAKE_COMMAND}" -E copy
        "$<TARGET_FILE:oxide>" "${PROJECT_SOURCE_DIR}/README.md" "${PROJECT_SOURCE_DIR}/LICENSE"
        "${package_stage}"
    COMMAND "${CMAKE_COMMAND}" -E copy ${package_examples} "${package_stage}/examples"
    COMMAND "${CMAKE_COMMAND}" -E copy
        "${PROJECT_SOURCE_DIR}/third_party/fbink/LICENSE" "${package_stage}/licenses/fbink.txt"
    COMMAND "${CMAKE_COMMAND}" -E copy
        "${PROJECT_SOURCE_DIR}/third_party/fbink/CREDITS" "${package_stage}/licenses/fbink-credits.txt"
    COMMAND "${CMAKE_COMMAND}" -E copy
        "${PROJECT_SOURCE_DIR}/third_party/fbink/font8x8/README" "${package_stage}/licenses/font8x8.txt"
    ${package_commands}
    COMMAND "${CMAKE_COMMAND}" -E chdir "${PROJECT_BINARY_DIR}/package"
        "${CMAKE_COMMAND}" -E tar czf "${package_archive}" --format=gnutar oxide
    DEPENDS ${package_dependencies} "${CMAKE_CURRENT_LIST_FILE}"
    COMMENT "Packaging oxide-${OXIDE_ARCH}.tar.gz"
    VERBATIM
)
add_custom_target(dist DEPENDS "${package_archive}")
