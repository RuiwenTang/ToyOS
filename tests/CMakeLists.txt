# tests — host-side unit tests for the copied kernel modules.
#
# Header-only modules (atomic.h) are included straight from kernel/include;
# heap.c is compiled as C with its host path (no __TOYOS_KERNEL__), exactly
# the configuration ToyOS64's host tests use. bitmap.c is the verbatim
# upstream port (upstream host-tests it too); memmap.c compiles on the host
# for its pure memmap_build path — the DTB/serial glue is kernel-only.

add_library(toyos_kernel_headers INTERFACE)
target_include_directories(toyos_kernel_headers INTERFACE ${CMAKE_SOURCE_DIR}/kernel/include)

add_executable(toyos_host_tests
    test_atomic.cpp
    test_heap.cpp
    test_bitmap.cpp
    test_memmap.cpp
    ${CMAKE_SOURCE_DIR}/kernel/mm/heap.c
    ${CMAKE_SOURCE_DIR}/kernel/mm/bitmap.c
    ${CMAKE_SOURCE_DIR}/kernel/mm/memmap.c)
target_link_libraries(toyos_host_tests PRIVATE toyos_kernel_headers gtest_main)

include(GoogleTest)
gtest_discover_tests(toyos_host_tests)
