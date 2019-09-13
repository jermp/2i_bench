set (BENCHMARK_DATA_DIR "${CMAKE_CURRENT_SOURCE_DIR}/realdata/") # does this ever get used?

add_c_benchmark(create_benchmark)
add_c_benchmark(adversarialunions_benchmark)
# We exclude POSIX tests from Visual Studio default build
if(NOT WIN32)
    add_c_benchmark(real_bitmaps_benchmark)
    add_c_benchmark(real_bitmaps_contains_benchmark)
    add_c_benchmark(iteration_benchmark)
    add_c_benchmark(add_benchmark)
    target_link_libraries(add_benchmark m)
    add_c_benchmark(frozen_benchmark)

    # added benchmarks
    add_c_benchmark(build_from_ds2i)
    add_c_benchmark(create_ds2i_format)
    add_c_benchmark(pair_wise_intersect)
    add_c_benchmark(pair_wise_union)
    add_c_benchmark(decoding)
    add_c_benchmark(select)
    add_c_benchmark(contains)
    add_c_benchmark(next_geq)
    add_c_benchmark(convert_test_files)

endif()
add_c_benchmark(bitset_container_benchmark)
add_c_benchmark(array_container_benchmark)
add_c_benchmark(run_container_benchmark)
add_c_benchmark(equals_benchmark)
