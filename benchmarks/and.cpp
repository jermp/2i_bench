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
#include "../external/s_indexes/include/intersection.hpp"
#include "../external/s_indexes/include/intersection_many.hpp"
#include "../external/s_indexes/include/next_geq_enumerator.hpp"
#include "../external/s_indexes/include/contains.hpp"

using namespace ds2i;

template <typename Enum>
static uint64_t boolean_and_query(uint64_t num_docs, std::vector<Enum>& enums,
                                  std::vector<uint32_t>& out) {
    // non-decreasing size
    std::sort(enums.begin(), enums.end(),
              [](auto const& l, auto const& r) { return l.size() < r.size(); });
    uint64_t size = 0;
    uint64_t candidate = enums[0].docid();
    size_t i = 1;
    while (candidate < num_docs) {
        for (; i < enums.size(); ++i) {
            enums[i].next_geq(candidate);
            if (enums[i].docid() != candidate) {
                candidate = enums[i].docid();
                i = 0;
                break;
            }
        }
        if (i == enums.size()) {
            out[size++] = candidate;
            enums[0].next();
            candidate = enums[0].docid();
            i = 1;
        }
    }
    return size;
}

void perftest_slicing(const char* index_filename, uint32_t num_queries) {
    std::vector<term_id_vec> queries;
    queries.reserve(num_queries);
    term_id_vec q;

    uint32_t i = 0;
    while (read_query_and_remove_duplicates(q) and
           queries.size() < num_queries) {
        queries.push_back(q);
        ++i;
    }

    num_queries = queries.size();
    std::cout << "Executing " << num_queries << " AND queries" << std::endl;

    sliced::s_index index;
    index.mmap(index_filename);
    uint64_t num_docs = index.universe();
    std::vector<uint32_t> out(num_docs);
    size_t total = 0;

    std::vector<sliced::s_sequence> sequences;

    essentials::timer_type t;
    for (int run = 0; run != testing::runs; ++run) {
        t.start();

        for (uint32_t i = 0; i != num_queries; ++i) {
            uint64_t size = 0;
            // if (queries[i].size() == 1) {
            //     size = index[queries[i][0]].decode(out.data());
            // } else
            if (queries[i].size() == 2) {
                size = sliced::pairwise_intersection(
                    index[queries[i][0]], index[queries[i][1]], out.data());
            } else {
                sequences.clear();
                for (auto term : queries[i]) {
                    sequences.push_back(index[term]);
                }
                size = sliced::intersection(sequences, out.data());
            }
            total += size;
        }

        t.stop();
    }
    PRINT_TIME
}

template <typename Index>
void perftest(const char* index_filename, uint32_t num_queries) {
    using namespace ds2i;

    LOAD_INDEX

    std::vector<term_id_vec> queries;
    queries.reserve(num_queries);
    term_id_vec q;
    while (read_query_and_remove_duplicates(q) and
           queries.size() < num_queries) {
        queries.push_back(q);
    }

    num_queries = queries.size();
    std::cout << "Executing " << num_queries << " AND queries" << std::endl;

    uint64_t num_docs = index.num_docs();
    std::vector<uint32_t> out(num_docs);
    size_t total = 0;

    typedef typename Index::document_enumerator enum_type;
    std::vector<enum_type> qq;

    essentials::timer_type t;
    for (int run = 0; run != testing::runs; ++run) {
        t.start();
        for (uint32_t i = 0; i != num_queries; ++i) {
            qq.clear();
            for (auto term : queries[i]) {
                qq.push_back(index[term]);
            }
            uint64_t size = boolean_and_query(num_docs, qq, out);
            total += size;
        }
        t.stop();
    }
    PRINT_TIME
}

int main(int argc, const char** argv) {
    using namespace ds2i;

    int mandatory = 4;
    if (argc < mandatory) {
        std::cerr << argv[0]
                  << " index_type index_filename num_queries < query_log"
                  << std::endl;
        return 1;
    }

    std::string index_type = argv[1];
    const char* index_filename = argv[2];
    uint32_t num_queries = std::atoi(argv[3]);

    if (index_type == "slicing") {
        perftest_slicing(index_filename, num_queries);
        return 0;
    }

    if (false) {
#define LOOP_BODY(R, DATA, T)                                           \
    }                                                                   \
    else if (index_type == BOOST_PP_STRINGIZE(T)) {                     \
        perftest<BOOST_PP_CAT(T, _index)>(index_filename, num_queries); \
        /**/

        BOOST_PP_SEQ_FOR_EACH(LOOP_BODY, _, DS2I_INDEX_TYPES);
#undef LOOP_BODY
    } else {
        logger() << "ERROR: Unknown index type " << index_type << std::endl;
    }

    return 0;
}
