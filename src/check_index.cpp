#include <fstream>
#include <iostream>

#include "configuration.hpp"
#include "index_build_utils.hpp"
#include "index_types.hpp"
#include "util.hpp"
#include "verify_collection.hpp"

int main(int argc, char** argv) {
    if (argc < 4) {
        std::cerr << "Usage " << argv[0] << ":\n"
                  << "\t<index_type> <index_filename> <collection_filename>"
                  << std::endl;
        return 1;
    }

    using namespace ds2i;
    std::string index_type = argv[1];
    const char* index_filename = argv[2];
    const char* collection_filename = argv[3];

    binary_freq_collection input(collection_filename);

    if (false) {
#define LOOP_BODY(R, DATA, T)                                               \
    }                                                                       \
    else if (index_type == BOOST_PP_STRINGIZE(T)) {                         \
        verify_collection<binary_freq_collection, BOOST_PP_CAT(T, _index)>( \
            input, index_filename);

        BOOST_PP_SEQ_FOR_EACH(LOOP_BODY, _, DS2I_INDEX_TYPES);
#undef LOOP_BODY
    } else {
        logger() << "ERROR: Unknown type " << index_type << std::endl;
    }

    return 0;
}
