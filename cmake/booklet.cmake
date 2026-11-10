set(ECJ_JAR "$ENV{ECJ_JAR}" CACHE FILEPATH "Eclipse ECJ 3.32.0 JAR")
set(K4_FRAMEWORK_API "$ENV{K4_FRAMEWORK_API}" CACHE FILEPATH "Kindle 4.1.4 framework-api.jar")
foreach(input ECJ_JAR K4_FRAMEWORK_API)
    if(NOT EXISTS "${${input}}")
        message(FATAL_ERROR "Set ${input} to its JAR path")
    endif()
endforeach()
find_package(Java REQUIRED COMPONENTS Development)

set(booklet_jar "${PROJECT_BINARY_DIR}/oxide.jar")
set(booklet_launcher_jar "${PROJECT_BINARY_DIR}/oxide-booklet.jar")
set(booklet_classes "${PROJECT_BINARY_DIR}/classes")
set(booklet_manifest "${PROJECT_BINARY_DIR}/booklet.mf")
file(GENERATE OUTPUT "${booklet_manifest}" CONTENT
"Manifest-Version: 1.0
Main-Class: org.katadelos.oxide.ShellBooklet

")
add_custom_command(
    OUTPUT "${booklet_jar}" "${booklet_launcher_jar}"
    COMMAND "${CMAKE_COMMAND}" -E rm -rf "${booklet_classes}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${booklet_classes}"
    COMMAND "${Java_JAVA_EXECUTABLE}" -jar "${ECJ_JAR}"
        -source 1.4 -target 1.4 -encoding UTF-8
        -classpath "${K4_FRAMEWORK_API}" -d "${booklet_classes}"
        "${PROJECT_SOURCE_DIR}/src/kindle4/ShellBooklet.java"
    COMMAND "${Java_JAR_EXECUTABLE}" cf "${booklet_jar}" -C "${booklet_classes}" .
    COMMAND "${Java_JAR_EXECUTABLE}" cfm "${booklet_launcher_jar}" "${booklet_manifest}"
    DEPENDS src/kindle4/ShellBooklet.java "${ECJ_JAR}" "${K4_FRAMEWORK_API}"
        "${booklet_manifest}" "${CMAKE_CURRENT_LIST_FILE}"
    COMMENT "Building Kindle 4 booklet"
    VERBATIM
)
add_custom_target(booklet ALL DEPENDS "${booklet_jar}" "${booklet_launcher_jar}")
