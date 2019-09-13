EXECUTE_PROCESS(COMMAND git submodule update --init
                WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/..
                OUTPUT_QUIET
            )

add_subdirectory(succinct EXCLUDE_FROM_ALL)
add_subdirectory(FastPFor EXCLUDE_FROM_ALL)

# Add streamvbyte
include_directories(streamvbyte/include)
add_library(streamvbyte STATIC streamvbyte/src/streamvbyte_encode.c
                               streamvbyte/src/streamvbyte_decode.c
)

# Add maskedvbyte
include_directories(MaskedVByte/include)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c11 -march=native")
add_library(MaskedVByte STATIC MaskedVByte/src/varintdecode.c
                               MaskedVByte/src/varintencode.c
)
