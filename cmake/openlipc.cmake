if(NOT EXISTS "${PROJECT_SOURCE_DIR}/third_party/openlipc/include/openlipc.h")
    message(FATAL_ERROR "Initialize dependencies with ./scripts/checkout-deps")
endif()

oxide_import_library(kindle_lipc lipc lipc /usr/lib/liblipc.so)
target_include_directories(kindle_lipc SYSTEM PUBLIC
    "${PROJECT_SOURCE_DIR}/third_party/openlipc/include")
target_link_libraries(oxide PRIVATE kindle_lipc)
