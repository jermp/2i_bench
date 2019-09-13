#include <iostream>
#include <fstream>

#include "util.hpp"
#include "../external/s_indexes/external/mm_file/include/mm_file/mm_file.hpp"

void filter(char const* docids_filename, char const* output_filename) {
    using namespace ds2i;
    mm::file_source<uint32_t> input(docids_filename, mm::advice::sequential);
    uint32_t const* data = input.data();
    assert(data[0] == 1);
    std::cout << "universe size: " << data[1] << std::endl;

    std::ofstream output(output_filename);
    if (!output.is_open()) {
        std::cerr << "could not open output file" << std::endl;
        return;
    }

    uint32_t sequences = 0;
    for (size_t i = 2; i < input.size();) {
        uint32_t n = data[i];
        if (n > constants::min_size) {
            output.write(reinterpret_cast<char const*>(data + i),
                         (n + 1) * sizeof(uint32_t));
            ++sequences;
            if (sequences % 1000 == 0) {
                std::cout << "processed " << sequences << " sequences"
                          << std::endl;
            }
        }
        i += n + 1;
    }

    output.close();
}

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cout << argv[0] << " docids_filename output_filename" << std::endl;
        return 1;
    }

    filter(argv[1], argv[2]);
    return 0;
}