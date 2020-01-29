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
#include "../external/s_indexes/include/decode.hpp"

#define INDEXES                                                              \
    (pef_uniform)(pef_opt)(bic)(maskedvbyte)(optpfor)(simple16)(qmx)(delta)( \
        opt_delta)(delta_table)(rice)(single_packed_dint)(opt_vbyte)

void perftest_slicing(char const* index_filename) {
    using namespace sliced;

    s_index index;
    index.mmap(index_filename);

    std::vector<uint32_t> out(index.universe());
    uint64_t integers = 0;
    essentials::timer_type t;

    std::cout << "Decoding" << std::endl;
    t.start();
    for (size_t i = 0; i != index.size(); ++i) {
        auto sequence = index[i];
        size_t decoded = sequence.decode(out.data());
        integers += decoded;
    }
    t.stop();

    std::cout << "decoded " << index.size() << " sequences" << std::endl;
    std::cout << "decoded " << integers << " integers" << std::endl;

    double elapsed = t.average();
    std::cout << "Elapsed time: " << elapsed / 1000000 << " [sec]\n";
    std::cout << "Mean per sequence: " << elapsed / index.size()
              << " [musec]\n";
    std::cout << "Mean per integer: " << elapsed / integers * 1000 << " [ns]";
    std::cout << std::endl;
}

template <typename Index>
void perftest(const char* index_filename) {
    using namespace ds2i;

    LOAD_INDEX

    std::vector<uint32_t> out(index.num_docs());
    uint64_t integers = 0;
    essentials::timer_type t;

    std::cout << "Decoding" << std::endl;
    t.start();
    for (uint64_t i = 0; i != index.size(); ++i) {
        uint32_t decoded = index.decode(i, out.data());
        integers += decoded;
    }
    t.stop();

    std::cout << "decoded " << index.size() << " sequences" << std::endl;
    std::cout << "decoded " << integers << " integers" << std::endl;

    double elapsed = t.average();
    std::cout << "Elapsed time: " << elapsed / 1000000 << " [sec]\n";
    std::cout << "Mean per sequence: " << elapsed / index.size()
              << " [musec]\n";
    std::cout << "Mean per integer: " << elapsed / integers * 1000 << " [ns]";
    std::cout << std::endl;
}

int main(int argc, const char** argv) {
    using namespace ds2i;

    int mandatory = 3;
    if (argc < mandatory) {
        std::cerr << argv[0] << " index_type index_filename " << std::endl;
        return 1;
    }

    std::string index_type = argv[1];
    const char* index_filename = argv[2];

    if (index_type == "slicing") {
        perftest_slicing(index_filename);
        return 0;
    }

    if (false) {
#define LOOP_BODY(R, DATA, T)                              \
    }                                                      \
    else if (index_type == BOOST_PP_STRINGIZE(T)) {        \
        perftest<BOOST_PP_CAT(T, _index)>(index_filename); \
        /**/

        BOOST_PP_SEQ_FOR_EACH(LOOP_BODY, _, INDEXES);
#undef LOOP_BODY
    } else {
        logger() << "ERROR: Unknown index type " << index_type << std::endl;
    }

    return 0;
}
