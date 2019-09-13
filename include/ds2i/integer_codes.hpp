#pragma once

#include "bits_enumerator.hpp"
#include "succinct/bit_vector.hpp"

namespace ds2i {

void write_unary(succinct::bit_vector_builder& bvb, uint64_t n) {
    uint64_t hb = uint64_t(1) << n;
    bvb.append_bits(hb, n + 1);
}

uint64_t read_unary(succinct::bit_vector::enumerator& it) {
    return it.skip_zeros();
}

uint64_t read_unary(bits_enumerator& it) {
    return it.skip_zeros();
}

// note: n can be 0
void write_gamma(succinct::bit_vector_builder& bvb, uint64_t n) {
    uint64_t nn = n + 1;
    uint64_t l = succinct::broadword::msb(nn);
    uint64_t hb = uint64_t(1) << l;
    bvb.append_bits(hb, l + 1);
    bvb.append_bits(nn ^ hb, l);
}

void write_gamma_nonzero(succinct::bit_vector_builder& bvb, uint64_t n) {
    assert(n > 0);
    write_gamma(bvb, n - 1);
}

uint64_t read_gamma(succinct::bit_vector::enumerator& it) {
    uint64_t l = read_unary(it);
    return (it.take(l) | (uint64_t(1) << l)) - 1;
}

uint64_t read_gamma(bits_enumerator& it) {
    uint64_t l = read_unary(it);
    return (it.take(l) | (uint64_t(1) << l)) - 1;
}

uint64_t read_gamma_nonzero(succinct::bit_vector::enumerator& it) {
    return read_gamma(it) + 1;
}

void write_delta(succinct::bit_vector_builder& bvb, uint64_t n) {
    uint64_t nn = n + 1;
    uint64_t l = succinct::broadword::msb(nn);
    uint64_t hb = uint64_t(1) << l;
    write_gamma(bvb, l);
    bvb.append_bits(nn ^ hb, l);
}

void write_rice(succinct::bit_vector_builder& bvb, uint64_t n, const uint64_t k,
                const uint64_t divisor) {
    assert(k > 0 and k < 32);
    assert(divisor == uint64_t(1) << k);
    uint64_t q = n / divisor;
    write_delta(bvb, q);  // write quotient in delta
    uint64_t r = n - q * divisor;
    bvb.append_bits(r, k);
}

uint64_t read_delta(succinct::bit_vector::enumerator& it) {
    uint64_t l = read_gamma(it);
    return (it.take(l) | (uint64_t(1) << l)) - 1;
}

uint64_t read_delta(bits_enumerator& it) {
    uint64_t l = read_gamma(it);
    return (it.take(l) | (uint64_t(1) << l)) - 1;
}

uint64_t read_rice(bits_enumerator& it, const uint64_t k,
                   const uint64_t divisor) {
    assert(k > 0 and k < 32);
    assert(divisor == uint64_t(1) << k);
    uint64_t q = read_delta(it);
    uint64_t r = it.take(k);
    return r + q * divisor;
}

uint32_t reverse(uint32_t x, uint32_t len) {
    uint32_t r = 0;
    while (len) {
        r <<= 1;
        r |= x & 1;
        x >>= 1;
        --len;
    }
    return r;
}

void write_zeta(succinct::bit_vector_builder& bvb, uint64_t n,
                const uint64_t k) {
    assert(k > 0 and k < 32);

    uint64_t l = succinct::broadword::msb(++n);
    uint64_t h = l / k;
    write_unary(bvb, h);
    uint64_t left = uint64_t(1) << (h * k);
    uint32_t shrunk = n - left;

    if (shrunk < left) {
        uint32_t reversed = reverse(shrunk, h * k + k - 1);
        bvb.append_bits(reversed, h * k + k - 1);
    } else {
        uint32_t reversed = reverse(n, h * k + k);
        bvb.append_bits(reversed, h * k + k);
    }
}

uint64_t read_zeta(bits_enumerator& it, const uint64_t k) {
    assert(k > 0 and k < 32);

    uint64_t h = read_unary(it);
    uint64_t left = uint64_t(1) << (h * k);
    uint32_t read = it.take(h * k + k - 1);
    uint32_t shrunk = reverse(read, h * k + k - 1);

    if (shrunk < left) {
        return shrunk + left - 1;
    }

    return (shrunk << 1) + it.take(1) - 1;
}

}  // namespace ds2i
