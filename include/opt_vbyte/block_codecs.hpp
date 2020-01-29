#pragma once

#include "succinct/bit_vector.hpp"
#include "succinct/util.hpp"
#include "../ds2i/block_codecs.hpp"
#include "util.hpp"

namespace pvb {

uint32_t binary_bitsize(uint32_t x) {
    if (x == 0) return 1;
    assert(x > 0);
    return succinct::broadword::msb(x) + 1;
}

uint32_t gamma_bitsize(uint32_t x) {
    return 2 * binary_bitsize(x) - 1;
}

uint32_t delta_bitsize(uint32_t x) {
    uint32_t b = binary_bitsize(x);
    return gamma_bitsize(b) + b - 1;
}

template <typename Iterator>
std::vector<uint32_t> take_gaps(Iterator begin, uint64_t base, uint64_t n) {
    std::vector<uint32_t> gaps;
    gaps.reserve(n);
    uint32_t last_doc(-1);
    for (size_t i = 0; i != n; ++i) {
        uint32_t doc = *(begin + i) - base;
        gaps.push_back(doc - last_doc - 1);  // delta gap
        last_doc = doc;
    }
    return gaps;
}

struct delta_block {
    static const uint64_t block_size = constants::block_size;
    static const int type = 0;
    static const bool is_byte_aligned = false;

    static inline uint64_t posting_cost(uint32_t x, uint64_t base) {
        if (x == 0 or x - base == 0) return 1;
        assert(x >= base);
        return delta_bitsize(x - base);
    }

    template <typename Iterator>
    static void write(succinct::bit_vector_builder& bvb, Iterator begin,
                      uint64_t base, uint64_t universe, uint64_t n,
                      ds2i::global_parameters const&) {
        auto gaps = take_gaps(begin, base, n);
        std::vector<uint8_t> out;
        encode(gaps.data(), universe, n, out);
        for (uint8_t v : out) bvb.append_bits(v, 8);
    }

    static void encode(uint32_t const* in, uint32_t sum_of_values, size_t n,
                       std::vector<uint8_t>& out) {
        uint32_t blocks = n / block_size;
        for (uint32_t i = 0; i != blocks; ++i) {
            ds2i::delta_block::encode(in, sum_of_values, block_size, out);
            in += block_size;
        }
        uint32_t tail = n - blocks * block_size;
        ds2i::delta_block::encode(in, sum_of_values, tail, out);
    }

    static uint8_t const* decode(uint8_t const* in, uint32_t* out,
                                 uint32_t sum_of_values, size_t n) {
        assert(n <= block_size);
        return ds2i::delta_block::decode(in, out, sum_of_values, n);
    }
};

struct maskedvbyte_block {
    static const uint64_t block_size = constants::block_size;
    static const int type = 0;
    static const bool is_byte_aligned = true;

    static inline uint64_t posting_cost(uint32_t x, uint64_t base) {
        if (x == 0 or x - base == 0) return 8;
        assert(x >= base);
        return 8 * succinct::util::ceil_div(ceil_log2(x - base + 1), 7);
    }

    template <typename Iterator>
    static void write(succinct::bit_vector_builder& bvb, Iterator begin,
                      uint64_t base, uint64_t universe, uint64_t n,
                      ds2i::global_parameters const&) {
        auto gaps = take_gaps(begin, base, n);
        std::vector<uint8_t> out;
        encode(gaps.data(), universe, n, out);
        for (uint8_t v : out) bvb.append_bits(v, 8);
    }

    static void encode(uint32_t const* in, uint32_t sum_of_values, size_t n,
                       std::vector<uint8_t>& out) {
        ds2i::maskedvbyte_block::encode(in, sum_of_values, n, out);
    }

    static uint8_t const* decode(uint8_t const* in, uint32_t* out,
                                 uint32_t sum_of_values, size_t n) {
        return ds2i::maskedvbyte_block::decode(in, out, sum_of_values, n);
    }
};

}  // namespace pvb
