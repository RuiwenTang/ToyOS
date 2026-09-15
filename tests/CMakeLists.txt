# tests — host-side unit tests for the copied kernel modules.
#
# Header-only modules (atomic.h) are included straight from kernel/include;
# heap.c is compiled as C with its host path (no __TOYOS_KERNEL__), exactly
# the configuration ToyOS64's host tests use.

add_library(toyos_kernel_headers INTERFACE)
target_include_directories(toyos_kernel_headers INTERFACE ${CMAKE_SOURCE_DIR}/kernel/include)

add_executable(toyos_host_tests
    test_atomic.cpp
    test_heap.cpp
    ${CMAKE_SOURCE_DIR}/kernel/mm/heap.c)
target_link_libraries(toyos_host_tests PRIVATE toyos_kernel_headers gtest_main)

include(GoogleTest)
gtest_discover_tests(toyos_host_tests)
