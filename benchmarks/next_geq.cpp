#include <iostream>

#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>

#include <succinct/mapper.hpp>

#include "index_types.hpp"
#include "util.hpp"
#include "test_common.hpp"

#include "../external/s_indexes/external/mm_file/include/mm_file/mm_file.hpp"
#include "../external/s_indexes/external/essentials/include/essentials.hpp"
#include "../external/s_indexes/include/util.hpp"
#include "../external/s_indexes/include/s_index.hpp"
#include "../external/s_indexes/include/next_geq_enumerator.hpp"

using namespace ds2i;

void test_slicing(const char* index_filename, char const* collection_filename,
                  std::string const& type) {
    sliced::s_index index;
    index.mmap(index_filename);

    mm::file_source<uint32_t> input(collection_filename,
                                    mm::advice::sequential);
    uint32_t const* data = input.data();
    assert(data[0] == 1);

    uint32_t universe = data[1];
    std::vector<uint32_t> buf(universe);

    essentials::json_lines stats;
    stats.new_line();
    stats.add("type", type);
    essentials::timer_type t;
    const uint32_t num_lists = std::min<uint32_t>(1000, index.size());

    for (uint32_t jump_size = 1; jump_size <= 1024; jump_size *= 2) {
        uint32_t k = 0;
        uint32_t queries = 0;
        uint64_t total = 0;
        t.reset();

        for (size_t i = 2; i < input.size() and k != num_lists;) {
            uint32_t n = data[i];
            buf.clear();
            if (n > constants::min_size) {
                // copy in a local buffer to avoid IO
                std::copy(data + i + 1, data + i + n + 1, buf.begin());
                queries += n / jump_size;
                sliced::next_geq_enumerator e(index[k++]);
                total = 0;
                t.start();
                for (uint32_t pos = 1; pos < n; pos += jump_size) {
                    uint32_t lower_bound = buf[pos] - 1;
                    total += e.next_geq(lower_bound);
                }
                t.stop();
            }
            i += n + 1;
        }

        std::cout << total << std::endl;
        double elapsed_musecs = t.elapsed();
        stats.new_line();
        stats.add("jump_size", std::to_string(jump_size));
        stats.add("avg_ns_per_query",
                  std::to_string((elapsed_musecs * 1000.0) / queries));
    }

    stats.print();
}

template <typename Index>
void test(const char* index_filename, char const* collection_filename,
          std::string const& type) {
    LOAD_INDEX

    mm::file_source<uint32_t> input(collection_filename,
                                    mm::advice::sequential);
    uint32_t const* data = input.data();
    assert(data[0] == 1);

    uint32_t universe = data[1];
    std::vector<uint32_t> buf(universe);

    essentials::json_lines stats;
    stats.new_line();
    stats.add("type", type);
    essentials::timer_type t;
    const uint32_t num_lists = std::min<uint32_t>(1000, index.size());

    for (uint32_t jump_size = 1; jump_size <= 1024; jump_size *= 2) {
        uint32_t k = 0;
        uint32_t queries = 0;
        uint64_t total = 0;
        t.reset();

        for (size_t i = 2; i < input.size() and k != num_lists;) {
            uint32_t n = data[i];
            buf.clear();
            if (n > constants::min_size) {
                // copy in a local buffer to avoid IO
                std::copy(data + i + 1, data + i + n + 1, buf.begin());
                auto e = index[k++];
                queries += n / jump_size;
                t.start();
                for (uint32_t pos = 1; pos < n; pos += jump_size) {
                    uint32_t lower_bound = buf[pos] - 1;
                    e.next_geq(lower_bound);
                    total += e.docid();
                }
                t.stop();
            }
            i += n + 1;
        }

        std::cout << total << std::endl;
        double elapsed_musecs = t.elapsed();
        stats.new_line();
        stats.add("jump_size", std::to_string(jump_size));
        stats.add("avg_ns_per_query",
                  std::to_string((elapsed_musecs * 1000.0) / queries));
    }

    stats.print();
}

int main(int argc, const char** argv) {
    int mandatory = 4;
    if (argc < mandatory) {
        std::cerr << "Usage: " << argv[0] << ":\n"
                  << "\t index_type index_filename collection_filename"
                  << std::endl;
        return 1;
    }

    std::string index_type = argv[1];
    const char* index_filename = argv[2];
    const char* collection_filename = argv[3];

    if (index_type == "slicing") {
        test_slicing(index_filename, collection_filename, index_type);
        return 0;
    }

    if (false) {
#define LOOP_BODY(R, DATA, T)                                              \
    }                                                                      \
    else if (index_type == BOOST_PP_STRINGIZE(T)) {                        \
        test<BOOST_PP_CAT(T, _index)>(index_filename, collection_filename, \
                                      index_type);                         \
        /**/

        BOOST_PP_SEQ_FOR_EACH(LOOP_BODY, _, DS2I_INDEX_TYPES);
#undef LOOP_BODY
    } else {
        logger() << "ERROR: Unknown index type " << index_type << std::endl;
    }

    return 0;
}
