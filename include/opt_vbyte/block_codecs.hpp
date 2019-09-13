#pragma once

#include "succinct/bit_vector.hpp"
#include "succinct/util.hpp"

#include "util.hpp"

namespace pvb {

struct maskedvbyte_block {
    static const uint64_t block_size = constants::block_size;
    static const int type = 0;

    static inline uint64_t posting_cost(posting_type x, uint64_t base) {
        if (x == 0 or x - base == 0) {
            return 8;
        }

        assert(x >= base);
        return 8 *
               succinct::util::ceil_div(ceil_log2(x - base + 1),  // delta gap
                                        7);
    }

    template <typename Iterator>
    static DS2I_FLATTEN_FUNC uint64_t
    bitsize(Iterator begin, ds2i::global_parameters const& params,
            uint64_t universe, uint64_t n, uint64_t base = 0) {
        (void)params;
        (void)universe;
        uint64_t cost = 0;
        auto it = begin;
        for (uint64_t i = 0; i < n; ++i, ++it) {
            cost += posting_cost(*it, base);
            base = *it;
        }
        return cost;
    }

    template <typename Iterator>
    static void write(succinct::bit_vector_builder& bvb, Iterator begin,
                      uint64_t base, uint64_t universe, uint64_t n,
                      ds2i::global_parameters const& params) {
        (void)params;
        std::vector<uint32_t> gaps;
        gaps.reserve(n);
        uint32_t last_doc(-1);
        for (size_t i = 0; i < n; ++i) {
            uint32_t doc = *(begin + i) - base;
            gaps.push_back(doc - last_doc);  // delta gap
            last_doc = doc;
        }
        std::vector<uint8_t> out;
        encode(gaps.data(), universe, n, out);
        for (uint8_t v : out) {
            bvb.append_bits(v, 8);
        }
    }

    static void encode(uint32_t const* in, uint32_t sum_of_values, size_t n,
                       std::vector<uint8_t>& out) {
        (void)sum_of_values;
        size_t size = n;
        uint32_t* src = const_cast<uint32_t*>(in);
        std::vector<uint8_t> buf(2 * size * sizeof(uint32_t));
        size_t out_len = vbyte_encode(src, size, buf.data());
        out.insert(out.end(), buf.data(), buf.data() + out_len);
    }

    static uint8_t const* decode(uint8_t const* in, uint32_t* out,
                                 uint32_t sum_of_values, size_t n) {
        (void)sum_of_values;
        auto read = masked_vbyte_decode(in, out, n);
        return in + read;
    }
};

}  // namespace pvb
