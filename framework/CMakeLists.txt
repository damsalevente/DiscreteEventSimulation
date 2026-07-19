add_library(des_framework STATIC
  src/event_queue.c
  src/rng.c
  src/config.c
  src/validation.c
  src/json_save.c
  src/resource.c
  src/entity.c
  src/stats.c
  src/engine.c
  src/json_load.c
  src/des_mdf.c
)

target_include_directories(des_framework
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
)

if(UNIX)
  target_link_libraries(des_framework PUBLIC m)
endif()

install(TARGETS des_framework ARCHIVE DESTINATION lib)
install(DIRECTORY include/des DESTINATION include)
