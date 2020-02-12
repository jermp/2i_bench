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

size_t three_terms_and_query(sliced::s_sequence const& s1,
                             sliced::s_sequence const& s2,
                             sliced::s_sequence const& s3,
                             std::vector<uint32_t>& out) {
    uint64_t size = sliced::pairwise_intersection(s1, s2, out.data());
    size_t k = 0;
    for (uint64_t i = 0; i != size; ++i) {
        uint32_t value = out[i];
        out[k] = value;
        k += s3.contains(value);
    }
    return k;
}

// version with next_geq
size_t boolean_and_query(uint64_t num_docs,
                         std::vector<sliced::next_geq_enumerator>& enums,
                         std::vector<uint32_t>& out) {
    // non-decreasing size
    std::sort(enums.begin(), enums.end(),
              [](auto const& l, auto const& r) { return l.size() < r.size(); });

    uint64_t size = 0;
    uint64_t candidate = enums[0].next_geq(0);
    size_t i = 1;
    while (candidate < num_docs) {
        for (; i < enums.size(); ++i) {
            uint64_t val = enums[i].next_geq(candidate);
            if (val != candidate) {
                candidate = val;
                i = 0;
                break;
            }
        }

        if (i == enums.size()) {
            out[size] = candidate;
            ++size;
            candidate = enums[0].next_geq(candidate + 1);
            i = 1;
        }
    }

    return size;
}

void perftest_slicing(const char* index_filename, uint32_t num_queries) {
    std::vector<term_id_vec> queries;
    queries.reserve(num_queries);
    term_id_vec q;
    while (read_query_and_remove_duplicates(q) and
           queries.size() < num_queries) {
        queries.push_back(q);
    }

    num_queries = queries.size();
    std::cout << "Executing " << num_queries << " AND queries" << std::endl;

    sliced::s_index index;
    index.mmap(index_filename);

    uint64_t num_docs = index.universe();
    std::vector<uint32_t> out(num_docs);
    size_t total = 0;

    // std::vector<sliced::next_geq_enumerator> enums;
    // std::vector<sliced::s_sequence> seqs(3);

    std::vector<sliced::s_sequence> sequences;

    essentials::timer_type t;
    for (int run = 0; run != testing::runs; ++run) {
        t.start();

        // 1. always next_geq
        // for (uint32_t i = 0; i != num_queries; ++i) {
        //     enums.clear();
        //     for (auto term : queries[i]) {
        //         enums.emplace_back(index[term]);
        //     }
        //     uint64_t size = boolean_and_query(num_docs, enums, out);
        //     total += size;
        // }

        // 2. only pairwise
        // for (uint32_t i = 0; i != num_queries; ++i) {
        //     auto const& query = queries[i];
        //     // total += sliced::pairwise_intersection(index[query[0]],
        //     // index[query[1]], out.data());
        //     enums.clear();
        //     enums.emplace_back(query[0]);
        //     enums.emplace_back(query[1]);
        //     total += boolean_and_query(num_docs, enums, out);
        // }

        // 3. mixed strategy with special cases for 2 and 3 terms
        // for (uint32_t i = 0; i != num_queries; ++i) {
        //     uint64_t size = 0;
        //     auto const& query = queries[i];
        //     if (query.size() == 2) {
        //         size = sliced::pairwise_intersection(
        //             index[query[0]], index[query[1]], out.data());
        //     } else if (query.size() == 3) {
        //         seqs[0] = index[query[0]];
        //         seqs[1] = index[query[1]];
        //         seqs[2] = index[query[2]];
        //         std::sort(seqs.begin(), seqs.end(),
        //                   [](auto const& l, auto const& r) {
        //                       return l.size() < r.size();
        //                   });
        //         size = three_terms_and_query(seqs[0], seqs[1], seqs[2], out);
        //     } else {
        //         enums.clear();
        //         for (auto term : query) {
        //             enums.push_back(index[term]);
        //         }
        //         size = boolean_and_query(num_docs, enums, out);
        //     }
        //     total += size;
        // }

        // 4. intersection with many
        for (uint32_t i = 0; i != num_queries; ++i) {
            sequences.clear();
            for (auto term : queries[i]) {
                sequences.push_back(index[term]);
            }
            uint64_t size = sliced::intersection(sequences, out.data());
            total += size;
        }

        t.stop();
    }
    PRINT_TIME
}

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
            out[size] = candidate;
            ++size;
            enums[0].next();
            candidate = enums[0].docid();
            i = 1;
        }
    }

    return size;
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
