#pragma once

#include <boost/progress.hpp>
#include <boost/filesystem.hpp>
#include <unordered_map>

#include "dint_configuration.hpp"
#include "statistics_collectors.hpp"
#include "binary_blocks_collection.hpp"
#include "hash_utils.hpp"
#include "util.hpp"

namespace ds2i {

static const double codeword_bits = std::log2(constants::num_entries);
static const double initial_bpi = 3 * codeword_bits;
static const double eps = 0.0001;

double cost(uint32_t block_size, uint32_t block_frequency) {
    return block_frequency * (initial_bpi * block_size - codeword_bits);
}

double compute_saving(uint32_t block_size, uint32_t block_frequency,
                      uint64_t total_integers) {
    return cost(block_size, block_frequency) / total_integers;
};

struct cost_filter {
    cost_filter(double threshold = eps) : m_threshold(threshold) {}

    bool operator()(block_type const& block, uint64_t total_integers) const {
        return compute_saving(block.data.size(), block.freq, total_integers) >
               m_threshold;
    }

private:
    double m_threshold;
};

template <typename Dictionary, typename Statistics>
struct decreasing_static_frequencies {
    typedef Dictionary dictionary_type;
    typedef Statistics statistics_type;

    static std::string type() {
        return "DSF-" + std::to_string(dictionary_type::num_entries) + "-" +
               std::to_string(dictionary_type::max_entry_size);
    }

    static auto filter() {
        cost_filter filter(eps / 1000);
        return filter;
    }

    static void build(typename dictionary_type::builder& dict_builder,
                      statistics_type& stats) {
        logger() << "building " << type() << " dictionary for "
                 << stats.total_integers << " integers" << std::endl;

        dict_builder.init();
        for (uint64_t s = 0; s != stats.blocks.size(); ++s) {
            uint64_t n = dictionary_type::num_entries;
            if (stats.blocks[s].size() < n) {
                n = stats.blocks[s].size();
            }

            auto it = stats.blocks[s].begin();
            for (uint64_t i = 0; i != n; ++i, ++it) {
                auto const& block = *it;
                dict_builder.append(block.data.data(), block.data.size(), s);
            }
        }

        dict_builder.build();
    }
};
}  // namespace ds2i
