add_executable(desim main.c)
target_link_libraries(desim PRIVATE des_framework)
install(TARGETS desim RUNTIME DESTINATION bin)
