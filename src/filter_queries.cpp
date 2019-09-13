#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <unordered_map>

typedef uint32_t term_id_type;
typedef std::vector<term_id_type> term_id_vec;

bool read_query(term_id_vec& ret, std::istream& is = std::cin) {
    ret.clear();
    std::string line;
    if (!std::getline(is, line))
        return false;
    std::istringstream iline(line);
    term_id_type term_id;
    while (iline >> term_id) {
        ret.push_back(term_id);
    }
    return true;
}

int main(int argc, const char** argv) {
    int mandatory = 2;
    if (argc < mandatory) {
        std::cerr << argv[0]
                  << " term_map_filename"
                     " < query_log"
                  << std::endl;
        return 1;
    }

    // NOTE: term map assumed to be formatted as
    // <original_term_id> <mapped_term_id>
    const char* term_map_filename = argv[1];

    std::unordered_map<term_id_type, term_id_type> term_map;
    {
        std::ifstream term_map_file(term_map_filename);
        if (!term_map_file.is_open()) {
            std::cerr << "could not open file '" << term_map_filename << "'"
                      << std::endl;
            return 1;
        }

        while (term_map_file) {
            term_id_type original;
            term_id_type mapped;
            term_map_file >> original;
            term_map_file >> mapped;
            term_map[original] = mapped;
        }
    }

    term_id_vec q;
    while (read_query(q)) {
        if (q.size() == 2 and term_map.find(q[0]) != term_map.cend() and
            term_map.find(q[1]) != term_map.cend()) {
            std::cout << term_map[q[0]] << " " << term_map[q[1]] << std::endl;
        }
    }

    return 0;
}