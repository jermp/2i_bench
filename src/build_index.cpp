#include <fstream>
#include <iostream>
#include <algorithm>
#include <thread>
#include <numeric>

#include <succinct/mapper.hpp>

#include "configuration.hpp"
#include "index_types.hpp"
#include "util.hpp"
#include "verify_collection.hpp"
#include "index_build_utils.hpp"

#include "../external/s_indexes/external/essentials/include/essentials.hpp"
#include "../external/s_indexes/include/s_index.hpp"
#include "../external/s_indexes/include/builder.hpp"

void create_collection_slicing(sliced::parameters const& params,
                               char const* output_filename) {
    using namespace sliced;
    typedef s_index::builder builder_type;
    builder_type builder(params);
    essentials::timer_type t;
    t.start();
    auto stats = builder.build();
    t.stop();
    std::cerr << "collection built in " << t.average() / 1000000 << " [sec]"
              << std::endl;
    stats.print();
    if (output_filename) {
        essentials::print_size(builder);
        essentials::save<builder_type>(builder, output_filename);
    }
}

using namespace ds2i;

template <typename CollectionType>
void build_model(std::string input_basename,
                 typename CollectionType::builder& builder) {
    builder.build_model(input_basename);
}

template <typename CollectionType>
void create_collection(std::string input_basename,
                       global_parameters const& params,
                       const char* output_filename,
                       std::string const& seq_type) {
    binary_freq_collection input(input_basename.c_str());
    size_t num_docs = input.num_docs();

    typename CollectionType::builder builder(num_docs, params);
    build_model<CollectionType>(input_basename, builder);

    std::cerr << "universe size: " << input.num_docs() << std::endl;
    progress_logger plog("Encoded");

    essentials::timer_type t;
    t.start();
    for (auto const& plist : input) {
        uint64_t n = plist.docs.size();
        if (n > constants::min_size) {
            uint64_t freqs_sum = std::accumulate(
                plist.freqs.begin(), plist.freqs.end(), uint64_t(0));
            builder.add_posting_list(n, plist.docs.begin(), plist.freqs.begin(),
                                     freqs_sum);
            plog.done_sequence(n);
        }
    }
    t.stop();
    double elapsed_secs = t.average() / 1000000;
    std::cerr << "collection built in " << elapsed_secs << " [sec]"
              << std::endl;

    plog.log();
    CollectionType coll;
    builder.build(coll);

    stats_line()("type", seq_type)("worker_threads",
                                   configuration::get().worker_threads)(
        "construction_time", elapsed_secs);

    dump_stats(coll, seq_type, plog.postings);

    if (output_filename) {
        succinct::mapper::freeze(coll, output_filename);
    }
}

int main(int argc, const char** argv) {
    int mandatory = 3;
    if (argc < mandatory) {
        std::cerr << "Usage: " << argv[0] << ":\n"
                  << "\t index_type collection_basename "
                     "--out output_filename"
                  << std::endl;
        return 1;
    }

    std::string index_type = argv[1];
    const char* input_basename = argv[2];
    const char* output_filename = nullptr;
    if (argc > mandatory and std::string(argv[mandatory]) == "--out") {
        output_filename = argv[mandatory + 1];
    }

    if (index_type == "slicing") {
        sliced::parameters params;
        params.collection_filename = input_basename;
        params.collection_filename += ".docs";
        params.size = constants::min_size;
        create_collection_slicing(params, output_filename);
        return 0;
    }

    ds2i::global_parameters params;
    params.log_partition_size = configuration::get().log_partition_size;

    if (false) {
#define LOOP_BODY(R, DATA, T)                                     \
    }                                                             \
    else if (index_type == BOOST_PP_STRINGIZE(T)) {               \
        create_collection<BOOST_PP_CAT(T, _index)>(               \
            input_basename, params, output_filename, index_type); \
        /**/

        BOOST_PP_SEQ_FOR_EACH(LOOP_BODY, _, DS2I_INDEX_TYPES);
#undef LOOP_BODY
    } else {
        logger() << "ERROR: Unknown index type " << index_type << std::endl;
    }

    return 0;
}
