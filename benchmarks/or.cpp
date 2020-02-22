#include <iostream>

#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>

#include <succinct/mapper.hpp>

#include "index_types.hpp"
#include "util.hpp"
#include "test_common.hpp"

#include "../external/s_indexes/include/util.hpp"
#include "../external/s_indexes/include/s_index.hpp"
#include "../external/s_indexes/include/union.hpp"
#include "../external/s_indexes/include/union_many.hpp"
#include "../external/s_indexes/include/enumerator.hpp"

using namespace ds2i;

void perftest_slicing(const char* index_filename, uint32_t num_queries,
                      essentials::json_lines& log) {
    std::vector<term_id_vec> queries;
    queries.reserve(num_queries);
    term_id_vec q;
    while (read_query_and_remove_duplicates(q) and
           queries.size() < num_queries) {
        queries.push_back(q);
    }

    num_queries = queries.size();
    log.add("num_queries", std::to_string(num_queries));
    // assume all queries have the same number of terms...
    log.add("query_terms", std::to_string(queries.front().size()));

    sliced::s_index index;
    index.mmap(index_filename);

    uint64_t num_docs = index.universe();
    std::vector<uint32_t> out(num_docs);
    size_t total = 0;

    std::vector<sliced::s_sequence> sequences;

    std::cout << "Executing " << num_queries << " OR queries" << std::endl;
    essentials::timer_type t;
    for (int run = 0; run != testing::runs; ++run) {
        t.start();

        for (uint32_t i = 0; i != num_queries; ++i) {
            uint64_t size = 0;
            // if (queries[i].size() == 1) {
            //     size = index[queries[i][0]].decode(out.data());
            // } else
            if (queries[i].size() == 2) {
                size = sliced::pairwise_union(index[queries[i][0]],
                                              index[queries[i][1]], out.data());
            } else {
                sequences.clear();
                for (auto term : queries[i]) {
                    sequences.push_back(index[term]);
                }
                size = sliced::union_many(sequences, out.data());
            }
            total += size;
        }

        t.stop();
    }
    PRINT_TIME

    double avg_ms_per_query = (avg / num_queries) / 1000;
    log.add("avg_ms_per_query", std::to_string(avg_ms_per_query));
}

template <typename Enum>
size_t boolean_or_query(uint32_t num_docs, std::vector<Enum>& enums,
                        std::vector<uint32_t>& out) {
    uint32_t cur_doc = std::min_element(enums.begin(), enums.end(),
                                        [](auto const& l, auto const& r) {
                                            return l.docid() < r.docid();
                                        })
                           ->docid();
    size_t size = 0;
    while (cur_doc < num_docs) {
        out[size++] = cur_doc;
        uint32_t next_doc = num_docs;
        for (size_t i = 0; i != enums.size(); ++i) {
            if (enums[i].docid() == cur_doc) enums[i].next();
            if (enums[i].docid() < next_doc) {
                next_doc = enums[i].docid();
            }
        }
        cur_doc = next_doc;
    }
    return size;
}

template <typename Index>
void perftest(const char* index_filename, uint32_t num_queries,
              essentials::json_lines& log) {
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
    log.add("num_queries", std::to_string(num_queries));
    // assume all queries have the same number of terms...
    log.add("query_terms", std::to_string(queries.front().size()));

    uint64_t num_docs = index.num_docs();
    std::vector<uint32_t> out(num_docs);
    size_t total = 0;

    typedef typename Index::document_enumerator enum_type;
    std::vector<enum_type> qq;

    std::cout << "Executing " << num_queries << " OR queries" << std::endl;
    essentials::timer_type t;
    for (int run = 0; run != testing::runs; ++run) {
        t.start();
        for (uint32_t i = 0; i != num_queries; ++i) {
            qq.clear();
            for (auto term : queries[i]) {
                qq.push_back(index[term]);
            }
            uint64_t size = boolean_or_query(num_docs, qq, out);
            total += size;
            // for (uint64_t i = 0; i != size; ++i) {
            //     std::cout << out[i] << "\n";
            // }
        }
        t.stop();
    }
    PRINT_TIME

    double avg_ms_per_query = (avg / num_queries) / 1000;
    log.add("avg_ms_per_query", std::to_string(avg_ms_per_query));
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

    essentials::json_lines log;
    log.new_line();
    log.add("index_type", index_type);
    log.add("query_type", "or");

    if (index_type == "slicing") {
        perftest_slicing(index_filename, num_queries, log);
        return 0;
    }

    if (false) {
#define LOOP_BODY(R, DATA, T)                                                \
    }                                                                        \
    else if (index_type == BOOST_PP_STRINGIZE(T)) {                          \
        perftest<BOOST_PP_CAT(T, _index)>(index_filename, num_queries, log); \
        /**/

        BOOST_PP_SEQ_FOR_EACH(LOOP_BODY, _, DS2I_INDEX_TYPES);
#undef LOOP_BODY
    } else {
        logger() << "ERROR: Unknown index type " << index_type << std::endl;
    }

    return 0;
}
