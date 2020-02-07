#pragma once

typedef uint32_t term_id_type;
typedef std::vector<term_id_type> term_id_vec;

#include "../include/ds2i/queries.hpp"

namespace ds2i {

namespace testing {
// first run is for warming up
static const int runs = 5 + 1;
}  // namespace testing

bool read_query_and_remove_duplicates(term_id_vec& ret,
                                      std::istream& is = std::cin) {
    ret.clear();
    std::string line;
    if (!std::getline(is, line)) return false;
    std::istringstream iline(line);
    term_id_type term_id;
    while (iline >> term_id) ret.push_back(term_id);
    remove_duplicate_terms(ret);
    return true;
}

#define LOAD_INDEX                                          \
    Index index;                                            \
    boost::iostreams::mapped_file_source m(index_filename); \
    succinct::mapper::map(index, m);

#define PRINT_TIME                                                  \
    std::cout << total << std::endl;                                \
    t.discard_first();                                              \
    double avg = t.average();                                       \
    std::cout << "Mean per run: " << avg / 1000 << " [millisec]\n"; \
    std::cout << "Mean per query: " << (avg / num_queries) / 1000   \
              << " [millisec]";                                     \
    std::cout << std::endl;

}  // namespace ds2i
