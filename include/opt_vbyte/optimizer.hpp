#pragma once

#include "util.hpp"

namespace pvb {

template <typename Encoder>
struct optimizer {
    typedef compact_ranked_bitvector bitvector;

    static const int invalid_block_type = -1;

    template <typename Iterator>
    struct block {
        block(Iterator b)
            : type(invalid_block_type)
            , begin(b) {}

        int type;
        Iterator begin;
    };

    template <typename Iterator>
    static std::vector<block<Iterator>> compute_partition(Iterator begin,
                                                          uint64_t n) {
        assert(Encoder::type == 0 and bitvector::type == 1);

        auto curr_begin = begin;   // begin of current block
        auto curr_bv_end = begin;  // end of bitvector block
        auto curr_en_end = begin;  // end of Encoder block
        auto end = begin + n;      // end of list
        auto it = begin;           // current position

        std::vector<block<Iterator>> partition;
        block<Iterator> curr_block(curr_begin);
        int last_block_type = invalid_block_type;

        int64_t best_bv_gain = 0;
        int64_t best_en_gain = 0;
        int64_t curr_gain = -1;
        int64_t F = int64_t(constants::fix_cost);
        int64_t T = F;

        posting_type curr;
        posting_type last = *begin;

        auto push_block = [&]() {
            last_block_type = curr_block.type;
            partition.push_back(curr_block);
            curr_block = block<Iterator>(curr_begin);
            T = 2 * F;
        };

        auto encode_bitvector = [&]() {
            curr_block.type = bitvector::type;
            curr_begin = curr_bv_end;
            curr_block.begin = curr_begin;
            push_block();
            curr_gain -= best_bv_gain;
            best_bv_gain = 0;
            best_en_gain = curr_gain;
            curr_en_end = it + 1;
            assert(curr_gain <= 0);
        };

        auto encode_with_enc = [&]() {
            curr_block.type = Encoder::type;
            curr_begin = curr_en_end;
            curr_block.begin = curr_begin;
            push_block();
            curr_gain -= best_en_gain;
            best_en_gain = 0;
            best_bv_gain = curr_gain;
            curr_bv_end = it + 1;
            assert(curr_gain >= 0);
        };

        auto close = [&]() {
            if (best_bv_gain > F and best_bv_gain - curr_gain > F) {
                encode_bitvector();
            }

            if (best_en_gain < -F and best_en_gain - curr_gain < -F) {
                encode_with_enc();
            }

            if (curr_gain > 0) {
                curr_bv_end = end;
                encode_bitvector();
            } else {
                curr_en_end = end;
                encode_with_enc();
            }
        };

        for (int64_t last_gain = 0; it != end;
             ++it, last = curr, last_gain = curr_gain) {
            curr = *it;
            curr_gain += Encoder::posting_cost(curr, last) -
                         bitvector::posting_cost(curr, last);

            if (curr_gain >= last_gain) {  // gain is not decreasing

                if (curr_gain > best_bv_gain) {
                    best_bv_gain = curr_gain;
                    curr_bv_end = it + 1;
                }

                assert(best_en_gain <= curr_gain);
                if (best_en_gain - curr_gain < -2 * F and best_en_gain < -T) {
                    encode_with_enc();
                }

            } else {  // gain is decreasing

                if (curr_gain <= best_en_gain) {
                    best_en_gain = curr_gain;
                    curr_en_end = it + 1;
                }

                assert(best_bv_gain >= curr_gain);
                if (best_bv_gain - curr_gain > 2 * F and best_bv_gain > T) {
                    encode_bitvector();
                }
            }
        }

        assert(it == end);
        close();

        return partition;
    }
};
}  // namespace pvb
