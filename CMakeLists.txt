cmake_minimum_required(VERSION 3.19)
project(ConcurrentCollections LANGUAGES CXX)

include(FetchContent)

add_library(${PROJECT_NAME} INTERFACE
        src/ccol/common.h
        src/ccol/double_buffer_queue.h
        src/ccol/trivial_vector.h
        src/ccol/sparse_vector.h
        src/ccol/spinlock.h
)

target_compile_features(${PROJECT_NAME} INTERFACE cxx_std_20)
target_include_directories(${PROJECT_NAME} INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/src/")

option("${PROJECT_NAME}_BUILD_TESTS" "Should tests be built for concurrent collections." ${PROJECT_IS_TOP_LEVEL})

if(${PROJECT_NAME}_BUILD_TESTS)

    fetchcontent_declare(
            Catch2
            GIT_REPOSITORY https://github.com/catchorg/Catch2.git
            GIT_TAG v3.4.0
    )

    fetchcontent_makeavailable(Catch2)

    set(TEST_PROJECT_NAME "${PROJECT_NAME}_Tests")
    add_executable(${TEST_PROJECT_NAME} tests/double_buffer_queue_test.cpp
            tests/trivial_vector_test.cpp
            tests/sparse_vector_test.cpp)
    target_link_libraries(${TEST_PROJECT_NAME} PRIVATE ${PROJECT_NAME})
    target_link_libraries(${TEST_PROJECT_NAME} PRIVATE Catch2::Catch2WithMain)
endif()
