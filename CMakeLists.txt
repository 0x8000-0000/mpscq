cmake_minimum_required(VERSION 3.16.3 FATAL_ERROR)

project (sbit_mpscq VERSION 0.5.0 LANGUAGES CXX DESCRIPTION "Signbit MPSC Queue")

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

set (THREADS_PREFER_PTHREAD_FLAG ON)
find_package (Threads REQUIRED)

#
# Testing
#
include (GoogleTest)
enable_testing ()

add_subdirectory (tests)

#
# Documentation
#
find_package (Doxygen REQUIRED)
set (DOXYGEN_INPUT_DIR ${PROJECT_SOURCE_DIR}/include)
set (DOXYGEN_OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/docs/doxygen)
set (DOXYGEN_INDEX_FILE ${DOXYGEN_OUTPUT_DIR}/html/index.html)
set (DOXYFILE_IN ${CMAKE_CURRENT_SOURCE_DIR}/docs/Doxyfile.in)
set (DOXYFILE_OUT ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile)

configure_file (${DOXYFILE_IN} ${DOXYFILE_OUT} @ONLY)

file (MAKE_DIRECTORY ${DOXYGEN_OUTPUT_DIR}) #Doxygen won't create this for us
add_custom_command (OUTPUT ${DOXYGEN_INDEX_FILE}
                    DEPENDS ${CAT_CUTIFIER_PUBLIC_HEADERS}
                    COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYFILE_OUT}
                    MAIN_DEPENDENCY ${DOXYFILE_OUT} ${DOXYFILE_IN}
                    COMMENT "Generating docs")

add_custom_target (Doxygen ALL DEPENDS ${DOXYGEN_INDEX_FILE})
