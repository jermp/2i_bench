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
#include "../external/s_indexes/include/union.hpp"
#include "../external/s_indexes/include/enumerator.hpp"

using namespace ds2i;

// version with next
size_t boolean_or_query(uint32_t num_docs, size_t query_size,
                        std::vector<sliced::enumerator>& enums,
                        std::vector<uint32_t>& out) {
    uint32_t cur_doc =
        std::min_element(
            enums.begin(), enums.begin() + query_size,
            [](auto const& l, auto const& r) { return l.value() < r.value(); })
            ->value();

    size_t size = 0;
    while (cur_doc < num_docs) {
        out[size++] = cur_doc;
        uint32_t next_doc = num_docs;
        for (size_t i = 0; i != query_size; ++i) {
            if (enums[i].value() == cur_doc) enums[i].next();
            if (enums[i].value() < next_doc) {
                next_doc = enums[i].value();
            }
        }
        cur_doc = next_doc;
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
    std::cout << "Executing " << num_queries << " OR queries" << std::endl;

    sliced::s_index index;
    index.mmap(index_filename);

    uint64_t num_docs = index.universe();
    std::vector<uint32_t> out(num_docs);
    size_t total = 0;

    std::vector<sliced::enumerator> enums(20);

    essentials::timer_type t;
    for (int run = 0; run != testing::runs; ++run) {
        t.start();

        // 1. always next
        // for (auto const& query : queries) {
        //     for (uint64_t i = 0; i != query.size(); ++i) {
        //         enums[i].init(index[query[i]], num_docs);
        //     }
        //     uint64_t size =
        //         boolean_or_query(num_docs, query.size(), enums, out);
        //     total += size;
        // }

        // 2. only pairwise
        // for (uint32_t i = 0; i != num_queries; ++i) {
        //     auto const& query = queries[i];
        //     // total += sliced::pairwise_union(index[query[0]],
        //     // index[query[1]], out.data());
        //     enums.clear();
        //     enums.emplace_back(query[0]);
        //     enums.emplace_back(query[1]);
        //     total += boolean_or_query(num_docs, enums, out);
        // }

        // 3. mixed strategy with special cases for 2 terms
        for (auto const& query : queries) {
            uint64_t size = 0;
            if (query.size() == 2) {
                size = sliced::pairwise_union(index[query[0]], index[query[1]],
                                              out.data());
            } else {
                for (uint64_t i = 0; i != query.size(); ++i) {
                    enums[i].init(index[query[i]], num_docs);
                }
                size = boolean_or_query(num_docs, query.size(), enums, out);
            }
            total += size;
        }

        t.stop();
    }
    PRINT_TIME
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
    std::cout << "Executing " << num_queries << " OR queries" << std::endl;

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
            uint64_t size = boolean_or_query(num_docs, qq, out);
            total += size;
            // for (uint64_t i = 0; i != size; ++i) {
            //     std::cout << out[i] << "\n";
            // }
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
