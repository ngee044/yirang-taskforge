find_package(Boost REQUIRED COMPONENTS json filesystem system process)

add_executable(TaskForgeCoreTests
    CoreTests.cpp
    ../Main/MessageValidation.cpp
    ../Main/TaskDLQ.cpp
    ../Main/TaskExecutor.cpp
)
target_include_directories(TaskForgeCoreTests PRIVATE ../Main)
target_link_libraries(TaskForgeCoreTests PRIVATE Utilities Boost::json Boost::filesystem Boost::system Boost::process)

foreach(test_name IN ITEMS validation dlq executor)
    add_test(NAME ${test_name} COMMAND TaskForgeCoreTests ${test_name})
    set_tests_properties(${test_name} PROPERTIES TIMEOUT 20)
endforeach()

add_test(NAME task_scripts COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/task_scripts_test.sh ${PROJECT_SOURCE_DIR}/Main/tasks)
add_test(NAME startup_shutdown COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/startup_shutdown_test.sh $<TARGET_FILE:TaskForgeWorker>)
set_tests_properties(task_scripts PROPERTIES TIMEOUT 10)
set_tests_properties(startup_shutdown PROPERTIES TIMEOUT 35)
