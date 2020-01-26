#include <iostream>

#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>

#include <succinct/mapper.hpp>

#include "index_types.hpp"
#include "wand_data.hpp"
#include "queries.hpp"
#include "util.hpp"

const size_t runs = 10 + 1;

template <typename QueryOperator, typename IndexType>
void op_perftest(IndexType const& index,
                 QueryOperator&& query_op,  // XXX!!!
                 std::vector<ds2i::term_id_vec> const& queries,
                 std::string const& index_type, std::string const& query_type,
                 size_t runs) {
    using namespace ds2i;

    std::vector<double> query_times;
    size_t total = 0;
    for (size_t run = 0; run != runs; ++run) {
        auto tick = get_time_usecs();
        for (auto const& query : queries) {
            uint64_t results = query_op(index, query);
            // do_not_optimize_away(results);
            total += results;
        }
        double elapsed = double(get_time_usecs() - tick);
        if (run != 0) {  // first run is not timed
            query_times.push_back(elapsed);
        }
    }

    std::cout << total << std::endl;

    double avg_per_run =
        std::accumulate(query_times.begin(), query_times.end(), double(0.0)) /
        query_times.size();
    stats_line()("type", index_type)("query", query_type)(
        "avg_musec_per_query", avg_per_run / queries.size());
}

template <typename IndexType>
void perftest(const char* index_filename, const char* wand_data_filename,
              std::vector<ds2i::term_id_vec> const& queries,
              std::string const& type, std::string const& query_type) {
    using namespace ds2i;

    IndexType index;
    logger() << "Loading index from " << index_filename << std::endl;
    boost::iostreams::mapped_file_source m(index_filename);
    succinct::mapper::map(index, m);

    logger() << "Warming up posting lists" << std::endl;
    std::unordered_set<term_id_type> warmed_up;
    for (auto const& q : queries) {
        for (auto t : q) {
            if (!warmed_up.count(t)) {
                index.warmup(t);
                warmed_up.insert(t);
            }
        }
    }

    wand_data<> wdata;
    boost::iostreams::mapped_file_source md;
    if (wand_data_filename) {
        md.open(wand_data_filename);
        succinct::mapper::map(wdata, md, succinct::mapper::map_flags::warmup);
    }

    std::vector<std::string> query_types;
    boost::algorithm::split(query_types, query_type, boost::is_any_of(":"));

    for (auto const& t : query_types) {
        if (t == "and") {
            op_perftest(index, and_query<false>(), queries, type, t, runs);
        } else if (t == "and_freq") {
            op_perftest(index, and_query<true>(), queries, type, t, runs);
        } else if (t == "or") {
            op_perftest(index, or_query<false>(), queries, type, t, runs);
        } else if (t == "or_freq") {
            op_perftest(index, or_query<true>(), queries, type, t, runs);
        } else if (t == "wand" && wand_data_filename) {
            op_perftest(index, wand_query(wdata, 10), queries, type, t, runs);
        } else if (t == "ranked_and" && wand_data_filename) {
            op_perftest(index, ranked_and_query(wdata, 10), queries, type, t,
                        runs);
        } else if (t == "maxscore" && wand_data_filename) {
            op_perftest(index, maxscore_query(wdata, 10), queries, type, t,
                        runs);
        } else {
            logger() << "Unsupported query type: " << t << std::endl;
        }
    }
}

int main(int argc, const char** argv) {
    using namespace ds2i;

    int mandatory = 4;
    if (argc < mandatory) {
        std::cerr << argv[0]
                  << " <index_type> <query_type> <index_filename> "
                     "[wand_filename] < query_log"
                  << std::endl;
        return 1;
    }

    std::string type = argv[1];
    std::string query_type = argv[2];
    const char* index_filename = argv[3];
    const char* wand_data_filename = nullptr;

    if (argc > mandatory) {
        wand_data_filename = argv[mandatory];
    }

    std::vector<term_id_vec> queries;
    term_id_vec q;
    while (read_query(q)) queries.push_back(q);

    if (false) {
#define LOOP_BODY(R, DATA, T)                                                 \
    }                                                                         \
    else if (type == BOOST_PP_STRINGIZE(T)) {                                 \
        perftest<BOOST_PP_CAT(T, _index)>(index_filename, wand_data_filename, \
                                          queries, type, query_type);         \
        /**/

        BOOST_PP_SEQ_FOR_EACH(LOOP_BODY, _, DS2I_INDEX_TYPES);
#undef LOOP_BODY
    } else {
        logger() << "ERROR: Unknown type " << type << std::endl;
    }

    return 0;
}
