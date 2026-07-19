enable_testing()

function(add_des_test name source)
  add_executable(${name} ${source})
  target_link_libraries(${name} PRIVATE des_framework)
  target_include_directories(${name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
  add_test(NAME ${name} COMMAND ${name})
  set_tests_properties(${name} PROPERTIES WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
endfunction()

add_des_test(test_event_queue test_event_queue.c)
add_des_test(test_engine test_engine.c)
add_des_test(test_config test_config.c)
add_des_test(test_airport test_airport.c)
add_des_test(test_whatif test_whatif.c)
add_des_test(test_validation test_validation.c)
add_des_test(test_engine_edges test_engine_edges.c)

if(TARGET desim)
  add_test(NAME test_cli_replay
    COMMAND $<TARGET_FILE:desim> run ${PROJECT_SOURCE_DIR}/configs/coffee_shop.json
            --seed 42 --replay ${CMAKE_CURRENT_BINARY_DIR}/test_replay.json --json)
  add_test(NAME test_cli_sweep_throughput
    COMMAND $<TARGET_FILE:desim> sweep ${PROJECT_SOURCE_DIR}/configs/coffee_shop.json
            --resource Cashier=1:2 --runs 2 --seed 42 --objective "throughput>=0" --json)
  add_test(NAME test_cli_sweep_mean_flow
    COMMAND $<TARGET_FILE:desim> sweep ${PROJECT_SOURCE_DIR}/configs/coffee_shop.json
            --resource Cashier=1:2 --runs 2 --seed 42 --objective "mean-flow<=1000" --json)
  add_test(NAME test_cli_rejects_invalid_objective
    COMMAND $<TARGET_FILE:desim> sweep ${PROJECT_SOURCE_DIR}/configs/coffee_shop.json
            --resource Cashier=1:2 --objective latency=10)
  set_tests_properties(test_cli_rejects_invalid_objective PROPERTIES WILL_FAIL TRUE)
endif()
