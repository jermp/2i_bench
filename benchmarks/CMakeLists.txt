add_executable(and and.cpp)
target_link_libraries(and
  ${Boost_LIBRARIES}
  FastPFor_lib
  streamvbyte
  MaskedVByte
  )

add_executable(or or.cpp)
target_link_libraries(or
  ${Boost_LIBRARIES}
  FastPFor_lib
  streamvbyte
  MaskedVByte
  )

add_executable(access access.cpp)
target_link_libraries(access
  ${Boost_LIBRARIES}
  FastPFor_lib
  streamvbyte
  MaskedVByte
  )

add_executable(next_geq next_geq.cpp)
target_link_libraries(next_geq
  ${Boost_LIBRARIES}
  FastPFor_lib
  streamvbyte
  MaskedVByte
  )

add_executable(decode decode.cpp)
target_link_libraries(decode
  ${Boost_LIBRARIES}
  FastPFor_lib
  streamvbyte
  MaskedVByte
  )