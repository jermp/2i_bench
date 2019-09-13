#include <iostream>
#include <random>
#include <vector>
#include <chrono>

#include "util.hpp"
#include "dictionary_types.hpp"
#include "dint_configuration.hpp"

using namespace ds2i;

typedef single_dictionary_rectangular_type dictionary_type;

int main(int argc, char** argv) {
    if (argc < 1) {
        std::cerr << "Usage " << argv[0] << ":\n"
                  << "\t<dictionary_filename>" << std::endl;
        return 1;
    }

    char const* dictionary_filename = argv[1];

    dictionary_type dict;
    typename dictionary_type::builder builder;
    std::ifstream dictionary_file(dictionary_filename);
    builder.load(dictionary_file);
    uint64_t dict_size = builder.size();
    logger() << "loaded a dictionary with " << dict_size << " entries"
             << std::endl;
    builder.build(dict);

    constexpr uint64_t n = 10000000;
    std::random_device rd;
    std::default_random_engine eng(rd());
    std::uniform_int_distribution<uint32_t> uniform_dist(0, dict_size);

    std::vector<uint32_t> indexes;
    indexes.reserve(n);
    for (uint64_t i = 0; i < n; ++i) {
        indexes.push_back(uniform_dist(eng));
    }

    constexpr uint32_t runs = 10;
    std::vector<uint32_t> out(dictionary_type::max_entry_size,
                              0);  // output buffer
    double elapsed_time = 0;
    for (uint32_t run = 0; run < runs; ++run) {
        auto start = clock_type::now();
        for (auto index : indexes) {
            uint32_t decoded_ints = dict.copy(index, out.data());
            do_not_optimize_away(decoded_ints);
        }
        auto end = clock_type::now();
        std::chrono::nanoseconds elapsed = end - start;
        elapsed_time += elapsed.count();
    }

    logger() << "total elapsed time: " << elapsed_time / 1000000000 << " [secs]"
             << std::endl;
    logger() << "avg. time x run: " << elapsed_time / runs / 1000000000
             << " [secs]" << std::endl;
    logger() << "avg. time x copy: " << elapsed_time / runs / n << " [ns]"
             << std::endl;

    return 0;
}
