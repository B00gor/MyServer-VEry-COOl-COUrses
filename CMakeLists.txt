cmake_minimum_required(VERSION 3.5)
project(myServer CXX)

include(CheckIncludeFileCXX)

check_include_file_cxx(any HAS_ANY)
check_include_file_cxx(string_view HAS_STRING_VIEW)
check_include_file_cxx(coroutine HAS_COROUTINE)

if(NOT "${CMAKE_CXX_STANDARD}" STREQUAL "")
    # Keep user-defined standard
elseif(HAS_ANY AND HAS_STRING_VIEW AND HAS_COROUTINE)
    set(CMAKE_CXX_STANDARD 20)
elseif(HAS_ANY AND HAS_STRING_VIEW)
    set(CMAKE_CXX_STANDARD 17)
else()
    set(CMAKE_CXX_STANDARD 14)
endif()

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Main executable
add_executable(${PROJECT_NAME} main.cc config/config.json)

# Find Drogon (assumes installed via vcpkg, conan, or system package)
find_package(Drogon CONFIG REQUIRED)
find_library(BCRYPT_LIB bcrypt)
find_package(CURL REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBAV REQUIRED libavformat libavcodec libavutil)

# 🔥 ДОБАВЬТЕ ЭТИ СТРОКИ ДЛЯ ПОДКЛЮЧЕНИЯ FFMPEG
target_include_directories(${PROJECT_NAME} PRIVATE ${LIBAV_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME} PRIVATE ${LIBAV_LIBRARIES})
target_compile_options(${PROJECT_NAME} PRIVATE ${LIBAV_CFLAGS_OTHER})

target_link_libraries(${PROJECT_NAME} PRIVATE CURL::libcurl)
target_link_libraries(${PROJECT_NAME} PRIVATE Drogon::Drogon)
target_link_libraries(${PROJECT_NAME} PRIVATE ${BCRYPT_LIB} drogon)

# Enforce C++17+
if(CMAKE_CXX_STANDARD LESS 17)
    message(FATAL_ERROR "C++17 or higher is required")
elseif(CMAKE_CXX_STANDARD LESS 20)
    message(STATUS "Using C++17")
else()
    message(STATUS "Using C++20")
endif()

aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/controllers CTL_SRC)
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/models MODEL_SRC)

file(GLOB_RECURSE CTL_HEADERS CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/controllers/*.h)
file(GLOB_RECURSE MODEL_HEADERS CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/models/*.h)

target_sources(${PROJECT_NAME}
    PRIVATE
        ${CTL_SRC}
        ${MODEL_SRC}
        ${CTL_HEADERS}
        ${MODEL_HEADERS}
)

# Generate views (if you have a 'views' directory)
drogon_create_views(${PROJECT_NAME}
    ${CMAKE_CURRENT_SOURCE_DIR}/views
    ${CMAKE_CURRENT_BINARY_DIR}
)

# Include directories and sources
target_include_directories(${PROJECT_NAME}
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/models
)

target_sources(${PROJECT_NAME}
    PRIVATE
        ${CTL_SRC}
        ${MODEL_SRC}
)

# Optional: enable exports if using dynamic view loading
# set_property(TARGET ${PROJECT_NAME} PROPERTY ENABLE_EXPORTS ON)

# Build tests
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test/CMakeLists.txt)
    add_subdirectory(test)
endif()
