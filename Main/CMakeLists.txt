cmake_minimum_required(VERSION 3.18)

set(PROGRAM_NAME TaskForgeWorker)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)

project(${PROGRAM_NAME} VERSION 1.0.0.0)

set(HEADER_FILES
	Configurations.h
	MessageValidation.h
	TaskDLQ.h
	TaskExecutor.h
	TaskForgeWorker.h
)

set(SOURCE_FILES
	Configurations.cpp
	MessageValidation.cpp
	TaskDLQ.cpp
	TaskExecutor.cpp
	TaskForgeWorker.cpp
	main.cpp
)

add_executable(${PROGRAM_NAME} ${HEADER_FILES} ${SOURCE_FILES})

find_package(Boost REQUIRED COMPONENTS json filesystem system process)

target_link_libraries(${PROGRAM_NAME} PUBLIC Utilities Thread Redis AWSService Boost::json Boost::filesystem Boost::system Boost::process)
target_include_directories(${PROGRAM_NAME} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")

add_custom_command(TARGET ${PROGRAM_NAME} POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy_if_different
		"${CMAKE_CURRENT_SOURCE_DIR}/task_forge_worker_configurations.json"
		"$<TARGET_FILE_DIR:${PROGRAM_NAME}>/task_forge_worker_configurations.json"
	COMMAND ${CMAKE_COMMAND} -E copy_directory
		"${CMAKE_CURRENT_SOURCE_DIR}/tasks"
		"$<TARGET_FILE_DIR:${PROGRAM_NAME}>/tasks"
)
