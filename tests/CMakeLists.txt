include(FetchContent)
FetchContent_Declare(
  unity
  GIT_REPOSITORY https://github.com/ThrowTheSwitch/Unity.git
  GIT_TAG v2.6.1
)
set(UNITY_NO_EXTENSION ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(unity)

enable_testing()

add_executable(test_event_queue test_event_queue.c)
target_link_libraries(test_event_queue PRIVATE des_framework unity)
add_test(NAME test_event_queue COMMAND test_event_queue)

add_executable(test_engine test_engine.c)
target_link_libraries(test_engine PRIVATE des_framework unity)
add_test(NAME test_engine COMMAND test_engine)

add_executable(test_config test_config.c)
target_link_libraries(test_config PRIVATE des_framework unity)
add_test(NAME test_config COMMAND test_config)

add_executable(test_airport test_airport.c)
target_link_libraries(test_airport PRIVATE des_framework unity)
add_test(NAME test_airport COMMAND test_airport)

add_executable(test_whatif test_whatif.c)
target_link_libraries(test_whatif PRIVATE des_framework unity)
add_test(NAME test_whatif COMMAND test_whatif)
