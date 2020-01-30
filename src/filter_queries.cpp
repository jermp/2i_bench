#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <unordered_map>
#include <numeric>

typedef uint32_t term_id_type;
typedef std::vector<term_id_type> term_id_vec;

bool read_query(term_id_vec& ret, std::istream& is = std::cin) {
    ret.clear();
    std::string line;
    if (!std::getline(is, line)) return false;
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
        std::cerr << argv[0] << " term_map_filename < query_log" << std::endl;
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

    static const size_t max_query_size = 32;
    std::vector<uint32_t> query_size(max_query_size, 0);

    term_id_vec q;
    while (read_query(q)) {
        // only for 2 query terms
        // if (q.size() == 2 and term_map.find(q[0]) != term_map.cend() and
        //     term_map.find(q[1]) != term_map.cend()) {
        //     std::cout << term_map[q[0]] << " " << term_map[q[1]] <<
        //     std::endl;
        // }

        if (q.size() > 1) {
            bool in = true;
            for (auto t : q) {
                in &= term_map.find(t) != term_map.cend();
                if (!in) break;
            }

            if (in) {
                if (q.size() >= max_query_size) {
                    std::cerr << "found a query with " << q.size() << " terms"
                              << std::endl;
                    return 1;
                }
                query_size[q.size()] += 1;
                std::cout << term_map[q.front()];
                for (size_t i = 1; i != q.size(); ++i) {
                    std::cout << " " << term_map[q[i]];
                }
                std::cout << "\n";
            }
        }
    }

    // print distribution of query size
    uint32_t total_queries =
        std::accumulate(query_size.begin(), query_size.end(), 0);
    for (uint32_t i = 0; i != max_query_size; ++i) {
        if (query_size[i]) {
            std::cerr << query_size[i] << " queries of size " << i << " ("
                      << (query_size[i] * 100.0) / total_queries << "%)"
                      << std::endl;
        }
    }

    return 0;
}