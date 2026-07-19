add_library(des_framework STATIC
  src/event_queue.c
  src/rng.c
  src/config.c
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
