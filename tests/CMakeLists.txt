add_executable (simple_test simple_test.cpp)

target_link_libraries (simple_test PUBLIC sbit_mpscq)
target_link_libraries (simple_test PUBLIC project_warnings)
target_link_libraries (simple_test PUBLIC CONAN_PKG::gtest)

gtest_discover_tests (simple_test)

#
# performance baseline
#
add_executable (logger_spdlog logger_spdlog.cpp)

target_link_libraries (logger_spdlog PUBLIC project_warnings)
target_link_libraries (logger_spdlog PUBLIC CONAN_PKG::fmt)
target_link_libraries (logger_spdlog PUBLIC CONAN_PKG::spdlog)
target_link_libraries (logger_spdlog PUBLIC PRIVATE Threads::Threads)

#
# performance test
#
add_executable (logger_sim logger_sim.cpp)

target_link_libraries (logger_sim PUBLIC sbit_mpscq)
target_link_libraries (logger_sim PUBLIC project_warnings)
target_link_libraries (logger_sim PUBLIC CONAN_PKG::fmt)
target_link_libraries (logger_sim PUBLIC PRIVATE Threads::Threads)

add_executable (logger_sim_stdio logger_sim_stdio.cpp)

target_link_libraries (logger_sim_stdio PUBLIC sbit_mpscq)
target_link_libraries (logger_sim_stdio PUBLIC project_warnings)
target_link_libraries (logger_sim_stdio PUBLIC CONAN_PKG::fmt)
target_link_libraries (logger_sim_stdio PUBLIC PRIVATE Threads::Threads)

