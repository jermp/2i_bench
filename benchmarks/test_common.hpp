#pragma once

typedef uint32_t term_id_type;
typedef std::vector<term_id_type> term_id_vec;

bool read_query(term_id_vec& ret, std::istream& is = std::cin) {
    ret.clear();
    std::string line;
    if (!std::getline(is, line)) {
        return false;
    }

    std::istringstream iline(line);
    term_id_type term_id;
    while (iline >> term_id) {
        ret.push_back(term_id);
    }

    return true;
}

#define LOAD_INDEX                                          \
    Index index;                                            \
    boost::iostreams::mapped_file_source m(index_filename); \
    succinct::mapper::map(index, m);

#define PRINT_TIME                                                      \
    std::cout << total << std::endl;                                    \
    t.discard_first();                                                  \
    double avg = t.average();                                           \
    std::cout << "Mean per run: " << avg << " [musec]\n";               \
    std::cout << "Mean per query: " << avg / num_queries << " [musec]"; \
    std::cout << std::endl;
