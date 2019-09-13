#pragma once

#include "indexed_sequence.hpp"
#include "compact_ranked_bitvector.hpp"
#include "compact_elias_fano.hpp"

namespace ds2i {

template <typename Iterator>
uint32_t decode_sequence(Iterator& it, uint32_t* out) {
    uint32_t partitions = it.m_partitions;
    if (partitions == 1) {
        uint32_t base = it.m_cur_base;
        uint32_t n = it.cur_partition_size();
        it.move(0);
        for (uint32_t i = 0; i != n; ++i) {
            uint64_t val = it.next().second;
            out[i] = val + base;
        }
        return n;
    }

    uint32_t* in = out;
    compact_ranked_bitvector::enumerator bv_enum;
    compact_elias_fano::enumerator ef_enum;
    static const uint64_t type_bits = indexed_sequence::type_bits;

    for (uint32_t p = 0; p != partitions; ++p) {
        int t = it.partition_type(p);
        uint32_t n = it.cur_partition_size();
        uint32_t base = it.m_cur_base;
        switch (t) {
            case indexed_sequence::index_type::ranked_bitvector:
                bv_enum = compact_ranked_bitvector::enumerator(
                    *(it.m_bv),
                    it.m_sequences_offset + it.m_endpoint + type_bits,
                    it.m_cur_upper_bound - it.m_cur_base + 1, n, it.m_params);
                compact_ranked_bitvector::decode(bv_enum, base, out);
                break;
            case indexed_sequence::index_type::elias_fano:
                ef_enum = compact_elias_fano::enumerator(
                    *(it.m_bv),
                    it.m_sequences_offset + it.m_endpoint + type_bits,
                    it.m_cur_upper_bound - it.m_cur_base + 1, n, it.m_params);
                ef_enum.move(0);
                for (uint32_t i = 0; i != n; ++i) {
                    uint64_t val = ef_enum.read_next();
                    out[i] = val + base;
                }
                break;
            case indexed_sequence::index_type::all_ones:
                for (uint32_t i = 0; i != n; ++i) {
                    out[i] = i + base;
                }
                break;
            default:
                assert(false);
                __builtin_unreachable();
        }
        out += n;
    }
    return uint32_t(out - in);
}
}  // namespace ds2i