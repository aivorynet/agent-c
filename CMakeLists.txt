cmake_minimum_required(VERSION 3.16)
project(aivory-monitor VERSION 0.1.0 LANGUAGES C)
set(PROJECT_HOMEPAGE_URL "https://aivory.net/monitor/")

# Options
option(AIVORY_BUILD_SHARED "Build shared library" ON)
option(AIVORY_BUILD_STATIC "Build static library" ON)
option(AIVORY_BUILD_EXAMPLES "Build examples" OFF)

# Find dependencies
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBWEBSOCKETS REQUIRED libwebsockets)
pkg_check_modules(JANSSON REQUIRED jansson)

# Optional: libunwind for better backtraces
pkg_check_modules(LIBUNWIND libunwind)

# Source files
set(AIVORY_SOURCES
    src/agent.c
    src/config.c
    src/capture/signal_handler.c
    src/capture/backtrace.c
    src/capture/variable_capture.c
    src/transport/websocket.c
    src/transport/reporter.c
    src/util/json_builder.c
    src/util/hash.c
)

set(AIVORY_HEADERS
    include/aivory/monitor.h
    include/aivory/config.h
    include/aivory/types.h
)

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${LIBWEBSOCKETS_INCLUDE_DIRS}
    ${JANSSON_INCLUDE_DIRS}
)

if(LIBUNWIND_FOUND)
    include_directories(${LIBUNWIND_INCLUDE_DIRS})
    add_definitions(-DHAVE_LIBUNWIND)
endif()

# Compile flags
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
add_compile_options(-Wall -Wextra -Wpedantic)

# Build shared library
if(AIVORY_BUILD_SHARED)
    add_library(aivory-monitor-shared SHARED ${AIVORY_SOURCES})
    target_link_libraries(aivory-monitor-shared
        ${LIBWEBSOCKETS_LIBRARIES}
        ${JANSSON_LIBRARIES}
        pthread
    )
    if(LIBUNWIND_FOUND)
        target_link_libraries(aivory-monitor-shared ${LIBUNWIND_LIBRARIES})
    endif()
    set_target_properties(aivory-monitor-shared PROPERTIES
        OUTPUT_NAME aivory-monitor
        VERSION ${PROJECT_VERSION}
        SOVERSION 1
    )
endif()

# Build static library
if(AIVORY_BUILD_STATIC)
    add_library(aivory-monitor-static STATIC ${AIVORY_SOURCES})
    target_link_libraries(aivory-monitor-static
        ${LIBWEBSOCKETS_LIBRARIES}
        ${JANSSON_LIBRARIES}
        pthread
    )
    if(LIBUNWIND_FOUND)
        target_link_libraries(aivory-monitor-static ${LIBUNWIND_LIBRARIES})
    endif()
    set_target_properties(aivory-monitor-static PROPERTIES
        OUTPUT_NAME aivory-monitor
    )
endif()

# Install
install(DIRECTORY include/aivory DESTINATION include)
if(AIVORY_BUILD_SHARED)
    install(TARGETS aivory-monitor-shared
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
    )
endif()
if(AIVORY_BUILD_STATIC)
    install(TARGETS aivory-monitor-static
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
    )
endif()

# Examples
if(AIVORY_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()
