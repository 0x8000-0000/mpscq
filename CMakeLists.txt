cmake_minimum_required(VERSION 3.16.3 FATAL_ERROR)

project (sbit_mpscq VERSION 0.0.1 LANGUAGES CXX DESCRIPTION "Signbit MPSC Queue")

set (CMAKE_EXPORT_COMPILE_COMMANDS TRUE)
set (CMAKE_CXX_EXTENSIONS False)

include (${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup (TARGETS NO_OUTPUT_DIRS SKIP_STD)

add_library (project_options INTERFACE)
target_compile_features (project_options INTERFACE cxx_std_17)

add_library (project_warnings INTERFACE)
if (NOT MSVC)
   target_compile_options (project_warnings INTERFACE -Wall -Wextra -Werror -pedantic)
endif ()

add_library (sbit_mpscq INTERFACE)
target_include_directories (sbit_mpscq INTERFACE include)
target_link_libraries (sbit_mpscq INTERFACE project_options)

include (GoogleTest)
enable_testing ()

set (THREADS_PREFER_PTHREAD_FLAG ON)
find_package (Threads REQUIRED)

add_subdirectory (tests)
