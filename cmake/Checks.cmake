include_guard(GLOBAL)

function(oxide_add_checks)
    find_program(OXIDE_CLANG_FORMAT_EXECUTABLE NAMES clang-format-18
        DOC "LLVM 18 clang-format executable" NO_CMAKE_FIND_ROOT_PATH)
    find_program(OXIDE_CLANG_TIDY_EXECUTABLE NAMES clang-tidy-18
        DOC "LLVM 18 clang-tidy executable" NO_CMAKE_FIND_ROOT_PATH)
    find_program(OXIDE_RUN_CLANG_TIDY_EXECUTABLE NAMES run-clang-tidy-18 run-clang-tidy18
        DOC "LLVM 18 run-clang-tidy executable" NO_CMAKE_FIND_ROOT_PATH)
    set(OXIDE_LINT_JOBS "4" CACHE STRING "Maximum concurrent clang-tidy processes")
    set(OXIDE_CLANG_TIDY_EXTRA_ARGS "" CACHE STRING
        "Extra compiler arguments for clang-tidy, such as --gcc-toolchain=/path")

    file(GLOB_RECURSE format_sources CONFIGURE_DEPENDS
        "${PROJECT_SOURCE_DIR}/src/*.c" "${PROJECT_SOURCE_DIR}/src/*.cpp" "${PROJECT_SOURCE_DIR}/src/*.hpp"
        "${PROJECT_SOURCE_DIR}/cmake/imports/*.cpp")
    if(OXIDE_CLANG_FORMAT_EXECUTABLE)
        add_custom_target(format
            COMMAND "${OXIDE_CLANG_FORMAT_EXECUTABLE}" --style=file -i ${format_sources}
            WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
            COMMENT "Formatting project C and C++ sources"
            VERBATIM USES_TERMINAL)
        add_custom_target(format-check
            COMMAND "${OXIDE_CLANG_FORMAT_EXECUTABLE}" --style=file --dry-run --Werror
                ${format_sources}
            WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
            COMMENT "Checking project C and C++ formatting"
            VERBATIM USES_TERMINAL)
    else()
        foreach(target format format-check)
            add_custom_target(${target}
                COMMAND "${CMAKE_COMMAND}" -E echo
                    "LLVM 18 clang-format is required. Install it and reconfigure, or set OXIDE_CLANG_FORMAT_EXECUTABLE."
                COMMAND "${CMAKE_COMMAND}" -E false
                VERBATIM USES_TERMINAL)
        endforeach()
    endif()

    if(OXIDE_CLANG_TIDY_EXECUTABLE AND OXIDE_RUN_CLANG_TIDY_EXECUTABLE)
        set(extra_args ${OXIDE_CLANG_TIDY_EXTRA_ARGS})
        list(TRANSFORM extra_args PREPEND "-extra-arg=")
        add_custom_target(lint
            COMMAND "${OXIDE_RUN_CLANG_TIDY_EXECUTABLE}"
                -p "${PROJECT_BINARY_DIR}"
                -clang-tidy-binary "${OXIDE_CLANG_TIDY_EXECUTABLE}"
                -j "${OXIDE_LINT_JOBS}" ${extra_args}
                "^${PROJECT_SOURCE_DIR}/(src|cmake/imports)/.*\\.(c|cpp)$"
            WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
            COMMENT "Linting the configured C and C++ translation units"
            VERBATIM USES_TERMINAL)
    else()
        add_custom_target(lint
            COMMAND "${CMAKE_COMMAND}" -E echo
                "LLVM 18 clang-tidy and run-clang-tidy are required. Install them and reconfigure, or set OXIDE_CLANG_TIDY_EXECUTABLE and OXIDE_RUN_CLANG_TIDY_EXECUTABLE."
            COMMAND "${CMAKE_COMMAND}" -E false
            VERBATIM USES_TERMINAL)
    endif()
endfunction()
