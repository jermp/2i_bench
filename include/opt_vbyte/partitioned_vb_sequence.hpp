#pragma once

#include "partitioned_sequence_enumerator.hpp"
#include "integer_codes.hpp"
#include "util.hpp"
#include "indexed_sequence.hpp"

#include "optimizer.hpp"

#include <limits>
#include <cmath>

namespace pvb {

template <typename VByteBlockType>
struct partitioned_vb_sequence {
    typedef partitioned_sequence_enumerator<
        indexed_sequence<block_sequence<VByteBlockType>>>
        enumerator;

    typedef VByteBlockType VBBlock;
    typedef compact_ranked_bitvector RBBlock;

    template <typename Iterator>
    static void write(succinct::bit_vector_builder& bvb, Iterator begin,
                      uint64_t universe, uint64_t n,
                      ds2i::global_parameters const& params) {
        assert(n > 0);
        auto partition = optimizer<VByteBlockType>::compute_partition(begin, n);
        size_t partitions = partition.size();
        assert(partitions > 0);

        ds2i::write_gamma_nonzero(bvb, partitions);

        if (partitions == 1) {
            auto const& singleton = partition.front();
            uint64_t base = *begin;
            uint64_t universe_bits = ceil_log2(universe);
            bvb.append_bits(base, universe_bits);
            uint64_t relative_universe = *(begin + n - 1) - base;
            // write universe only if non-singleton and not tight
            if (n > 1) {
                if (base + relative_universe + 1 == universe) {
                    ds2i::write_delta(bvb, 0);  // tight universe
                } else {
                    ds2i::write_delta(bvb, relative_universe);
                }
            }

            push_pad(bvb);
            write_block(bvb, begin, singleton.type, base, *(begin + n - 1) + 1,
                        n, params);

        } else {
            succinct::bit_vector_builder bv_sequences;
            std::vector<uint64_t> sizes;
            sizes.reserve(partitions);
            std::vector<uint64_t> endpoints;
            endpoints.reserve(partitions);
            std::vector<uint64_t> upper_bounds;
            upper_bounds.reserve(partitions + 1);
            upper_bounds.push_back(*begin);

            auto b = begin;
            for (uint64_t prev_size = 0, i = 0; i < partitions; ++i) {
                uint64_t curr_base =
                    i == 0 ? *(begin) : *(partition[i - 1].begin - 1) + 1;
                uint64_t curr_n = std::distance(b, partition[i].begin);
                uint64_t curr_universe = *(b + curr_n - 1);

                write_block(bv_sequences, b, partition[i].type, curr_base,
                            curr_universe + 1, curr_n, params);

                sizes.push_back(prev_size + curr_n);
                endpoints.push_back(bv_sequences.size());
                upper_bounds.push_back(curr_universe);
                prev_size = sizes.back();
                b = partition[i].begin;
            }

            assert(upper_bounds.back() == *(begin + n - 1));
            assert(sizes.front() != 0);
            assert(sizes.back() == n);
            assert(sizes.size() == partitions);
            assert(endpoints.size() == partitions);
            assert(upper_bounds.size() == partitions + 1);

            succinct::bit_vector_builder bv_sizes;
            succinct::bit_vector_builder bv_upper_bounds;

            compact_elias_fano::write(bv_sizes, sizes.begin(), n,
                                      partitions - 1, params);
            compact_elias_fano::write(bv_upper_bounds, upper_bounds.begin(),
                                      universe, partitions + 1, params);
            uint64_t endpoint_bits = ceil_log2(bv_sequences.size() + 1);
            ds2i::write_gamma(bvb, endpoint_bits);

            bvb.append(bv_sizes);
            bvb.append(bv_upper_bounds);

            for (uint64_t p = 0; p < endpoints.size() - 1; ++p) {
                bvb.append_bits(endpoints[p], endpoint_bits);
            }

            push_pad(bvb);
            bvb.append(bv_sequences);
        }
    }

private:
    static const uint64_t type_bits = indexed_sequence<>::type_bits;

    template <typename Iterator>
    static void write_block(succinct::bit_vector_builder& bvb, Iterator begin,
                            int type, uint64_t base, uint64_t universe,
                            uint64_t n, ds2i::global_parameters const& params) {
        assert(n > 0);
        switch (type) {
            case VBBlock::type:
                bvb.append_bits(type, type_bits);
                push_pad(bvb);
                VBBlock::write(bvb, begin, base, universe, n, params);
                break;
            case RBBlock::type:
                bvb.append_bits(type, type_bits);
                RBBlock::write(bvb, begin, base, universe, n, params);
                break;
            default:
                assert(false);
        }
    }
};
}  // namespace pvb
