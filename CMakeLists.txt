cmake_minimum_required(VERSION 3.18)

set(PROGRAM_NAME yirang-taskforge)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/out")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

project(${PROGRAM_NAME} VERSION 1.0.0.0)

if(APPLE AND NOT CMAKE_CROSSCOMPILING)
    list(APPEND CMAKE_PREFIX_PATH /usr/local /opt/homebrew)
endif()

if(DEFINED VCPKG_TARGET_TRIPLET)
    list(APPEND CMAKE_PREFIX_PATH "${CMAKE_BINARY_DIR}/vcpkg_installed/${VCPKG_TARGET_TRIPLET}")
endif()

# CppToolkit 서브모듈: SQS/S3 + Redis + ThreadPool + Utilities만 사용
set(BUILD_AWS_LIB ON CACHE BOOL "Build aws library")
set(BUILD_NETWORK_LIB OFF CACHE BOOL "Build network library")
set(BUILD_DATABASE_LIB OFF CACHE BOOL "Build database library")
set(BUILD_RABBIT_MQ_LIB OFF CACHE BOOL "Build rabbitmq library")
set(BUILD_KAFKA_LIB OFF CACHE BOOL "Build kafka library")
set(BUILD_SAMPLES OFF CACHE BOOL "Build samples")

add_subdirectory(.CppToolkit)
add_subdirectory(Main)
