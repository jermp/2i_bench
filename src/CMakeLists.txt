add_executable(build_index build_index.cpp)
target_link_libraries(build_index
  ${Boost_LIBRARIES}
    FastPFor_lib
    streamvbyte
    MaskedVByte
  )

add_executable(create_wand_data create_wand_data.cpp)
target_link_libraries(create_wand_data
  ${Boost_LIBRARIES}
  )

add_executable(queries queries.cpp)
target_link_libraries(queries
  ${Boost_LIBRARIES}
  FastPFor_lib
  streamvbyte
  MaskedVByte
  )

add_executable(check_index check_index.cpp)
target_link_libraries(check_index
  ${Boost_LIBRARIES}
  FastPFor_lib
  streamvbyte
  MaskedVByte
  )

add_executable(dict_perf_test dict_perf_test.cpp)
target_link_libraries(dict_perf_test
  ${Boost_LIBRARIES}
  )

add_executable(filter_queries filter_queries.cpp)
add_executable(filter_lists filter_lists.cpp)
add_executable(print_statistics print_statistics.cpp)
target_link_libraries(print_statistics
  ${Boost_LIBRARIES}
  )