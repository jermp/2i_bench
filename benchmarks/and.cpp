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
#include "../external/s_indexes/include/next_geq_enumerator.hpp"

// first run is for warming up
static const int runs = 10 + 1;

// version with next_geq
size_t pairwise_intersection(uint64_t num_docs,
                             std::vector<sliced::next_geq_enumerator>& enums,
                             uint32_t* out) {
    uint64_t size = 0;
    uint64_t candidate = enums[0].next_geq(0);
    size_t i = 1;
    while (candidate < num_docs) {
        for (; i < 2; ++i) {
            uint64_t val = enums[i].next_geq(candidate);
            if (val != candidate) {
                candidate = val;
                i = 0;
                break;
            }
        }

        if (i == 2) {
            out[size] = candidate;
            ++size;
            candidate = enums[0].next_geq(candidate + 1);
            i = 1;
        }
    }

    return size;
}

void perftest_slicing(const char* index_filename, uint32_t num_queries) {
    // using namespace sliced;

    std::vector<sliced::query> queries;
    queries.reserve(num_queries);

    for (uint32_t i = 0; i != num_queries; ++i) {
        sliced::query q;
        int x = scanf("%d", &q.i);
        int y = scanf("%d", &q.j);
        if (x == EOF or y == EOF) {
            break;
        }
        queries.push_back(q);
    }

    sliced::s_index index;
    index.mmap(index_filename);

    uint64_t num_docs = index.universe();
    std::vector<uint32_t> out(num_docs);
    size_t total = 0;
    std::cout << "Executing " << num_queries << " pair-wise and queries"
              << std::endl;

    std::vector<sliced::next_geq_enumerator> enums(2);
    essentials::timer_type t;
    for (int run = 0; run != runs; ++run) {
        t.start();
        for (auto const& q : queries) {
            auto const& l = index[q.i];
            auto const& r = index[q.j];
            // total += sliced::pairwise_intersection(l, r, out.data());

            if (l.size() <= r.size()) {
                enums[0] = sliced::next_geq_enumerator(l);
                enums[1] = sliced::next_geq_enumerator(r);
                total += pairwise_intersection(num_docs, enums, out.data());
            } else {
                enums[0] = sliced::next_geq_enumerator(r);
                enums[1] = sliced::next_geq_enumerator(l);
                total += pairwise_intersection(num_docs, enums, out.data());
            }
        }
        t.stop();
    }
    PRINT_TIME
}

template <typename Enum>
static uint64_t pair_wise_and(uint64_t num_docs, std::vector<Enum>& enums,
                              std::vector<uint32_t>& out) {
    // non-decreasing size
    if (enums[0].size() > enums[1].size()) {
        std::swap(enums[0], enums[1]);
    }

    uint64_t size = 0;
    uint64_t candidate = enums[0].docid();
    size_t i = 1;
    while (candidate < num_docs) {
        for (; i < 2; ++i) {
            enums[i].next_geq(candidate);
            if (enums[i].docid() != candidate) {
                candidate = enums[i].docid();
                i = 0;
                break;
            }
        }

        if (i == 2) {
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
    term_id_vec q;
    while (read_query(q) and queries.size() < num_queries) {
        assert(q.size() == 2);
        queries.push_back(q);
    }

    num_queries = queries.size();
    std::cout << "Executing " << num_queries << " pair-wise and queries"
              << std::endl;

    uint64_t num_docs = index.num_docs();
    std::vector<uint32_t> out(num_docs);
    size_t total = 0;

    typedef typename Index::document_enumerator enum_type;
    std::vector<enum_type> qq;
    qq.reserve(2);

    essentials::timer_type t;
    for (int run = 0; run != runs; ++run) {
        t.start();
        for (uint32_t i = 0; i != num_queries; ++i) {
            qq.clear();
            for (auto term : queries[i]) {
                qq.push_back(index[term]);
            }
            uint64_t size = pair_wise_and(num_docs, qq, out);
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
