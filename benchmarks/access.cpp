#include <iostream>

#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>

#include <succinct/mapper.hpp>

#include "index_types.hpp"
#include "util.hpp"
#include "test_common.hpp"

#include "../external/s_indexes/external/essentials/include/essentials.hpp"
#include "../external/s_indexes/include/util.hpp"
#include "../external/s_indexes/include/s_index.hpp"
#include "../external/s_indexes/include/select.hpp"

// first run is for warming up
static const int runs = 3 + 1;

void perftest_slicing(char const* index_filename,
                      uint64_t num_queries_per_sequence) {
    using namespace sliced;

    s_index index;
    index.mmap(index_filename);

    uint64_t num_queries = index.size() * num_queries_per_sequence;
    std::vector<uint32_t> queries;
    queries.reserve(num_queries);

    for (uint64_t i = 0; i != num_queries; ++i) {
        uint32_t q = 0;
        std::cin >> q;
        queries.push_back(q);
    }

    std::cout << "Executing " << queries.size() << " access queries"
              << std::endl;

    size_t total = 0;
    essentials::timer_type t;
    for (int run = 0; run != runs; ++run) {
        uint64_t q = 0;
        uint32_t value = 0;
        t.start();
        for (uint32_t i = 0; i != index.size(); ++i) {
            auto sequence = index[i];
            for (uint32_t j = 0; j != num_queries_per_sequence; ++j) {
                sequence.select(queries[q++], value);
                total += value;
            }
        }
        t.stop();
    }
    PRINT_TIME
}

template <typename Index>
void perftest(const char* index_filename, uint64_t num_queries_per_sequence) {
    using namespace ds2i;

    LOAD_INDEX
    uint64_t num_queries = num_queries_per_sequence * index.size();
    std::vector<uint32_t> queries;
    queries.reserve(num_queries);

    for (uint64_t i = 0; i != num_queries; ++i) {
        uint32_t q = 0;
        std::cin >> q;
        queries.push_back(q);
    }

    std::cout << "Executing " << queries.size() << " access queries"
              << std::endl;

    size_t total = 0;
    typedef typename Index::document_enumerator enum_type;
    essentials::timer_type t;
    for (int run = 0; run != runs; ++run) {
        uint64_t q = 0;
        t.start();
        for (uint64_t i = 0; i != index.size(); ++i) {
            enum_type e = index[i];
            for (uint32_t j = 0; j != num_queries_per_sequence; ++j) {
                uint32_t position = queries[q++];
                assert(position < e.size());
                e.move(position);
                total += e.docid();
            }
        }
        t.stop();
    }
    PRINT_TIME
}

int main(int argc, const char** argv) {
    using namespace ds2i;

    int mandatory = 3;
    if (argc < mandatory) {
        std::cerr << argv[0]
                  << " index_type index_filename "
                     "num_queries_per_sequence < query_log"
                  << std::endl;
        return 1;
    }

    std::string index_type = argv[1];
    const char* index_filename = argv[2];
    uint64_t num_queries_per_sequence = std::stoull(argv[3]);

    if (index_type == "slicing") {
        perftest_slicing(index_filename, num_queries_per_sequence);
        return 0;
    }

    if (false) {
#define LOOP_BODY(R, DATA, T)                                        \
    }                                                                \
    else if (index_type == BOOST_PP_STRINGIZE(T)) {                  \
        perftest<BOOST_PP_CAT(T, _index)>(index_filename,            \
                                          num_queries_per_sequence); \
        /**/

        BOOST_PP_SEQ_FOR_EACH(LOOP_BODY, _, DS2I_INDEX_TYPES);
#undef LOOP_BODY
    } else {
        logger() << "ERROR: Unknown index type " << index_type << std::endl;
    }

    return 0;
}
