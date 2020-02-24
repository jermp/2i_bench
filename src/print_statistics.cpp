#include <iostream>

#include "binary_freq_collection.hpp"
#include "statistics.hpp"
#include "util.hpp"

using namespace ds2i;

template <typename Statistic>
void show_statistics(const char* collection_name) {
    uint64_t universe = 0;
    {
        std::string path(collection_name);
        size_t dot = path.find_last_of(".");
        binary_freq_collection coll(path.substr(0, dot).c_str());
        universe = coll.num_docs();
        std::cout << "universe: " << universe << std::endl;
    }
    Statistic stat(universe);
    binary_collection input(collection_name);
    auto it = input.begin();
    uint64_t total_postings = 0;
    uint64_t lists = 0;
    for (; it != input.end(); ++it) {
        auto const& list = *it;
        uint32_t n = list.size();
        if (n > constants::min_size) {
            auto doc = list.begin();
            for (uint64_t i = 0; i != n; ++i) stat.eat(*doc++);
            stat.init();
            total_postings += n;
            lists += 1;
            if (lists % 10000 == 0) {
                std::cout << "processed " << lists << " lists" << std::endl;
            }
        }
    }
    std::cout << "processed " << lists << " lists" << std::endl;
    std::cout << "processed " << total_postings << " integers" << std::endl;
    stat.finalize();
    std::cout << "bpi: " << static_cast<double>(stat.bits()) / total_postings
              << std::endl;
}

int main(int argc, const char** argv) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <type> <collection_name>"
                  << std::endl;
        std::cerr << "<type> must be one of: [gaps_entropy, gaps_logs]"
                  << std::endl;
        return 1;
    }

    std::string type(argv[1]);
    const char* collection_name = argv[2];

    if (type == "gaps_entropy") {
        show_statistics<gaps_entropy>(collection_name);
    } else if (type == "gaps_logs") {
        show_statistics<gaps_logs>(collection_name);
    } else {
        std::cerr << "unknown statistics type" << std::endl;
    }

    return 0;
}
