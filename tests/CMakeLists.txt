add_executable (simple_test simple_test.cpp)

target_link_libraries (simple_test PUBLIC sbit_mpscq)
target_link_libraries (simple_test PUBLIC project_warnings)
target_link_libraries (simple_test PUBLIC CONAN_PKG::gtest)

gtest_discover_tests (simple_test)
