#pragma once

#include "indexed_sequence.hpp"
#include "compact_ranked_bitvector.hpp"
#include "block_sequence.hpp"

namespace pvb {

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
    typedef block_sequence<maskedvbyte_block> block_sequence_t;
    typedef indexed_sequence<maskedvbyte_block> indexed_sequence_t;
    block_sequence_t::enumerator vb_enum;
    static const uint64_t type_bits = indexed_sequence_t::type_bits;

    for (uint32_t p = 0; p != partitions; ++p) {
        int t = it.partition_type(p);
        uint32_t n = it.cur_partition_size();
        uint32_t base = it.m_cur_base;
        switch (t) {
            case indexed_sequence_t::index_type::ranked_bitvector:
                bv_enum = compact_ranked_bitvector::enumerator(
                    *it.m_bv, it.m_sequences_offset + it.m_endpoint + type_bits,
                    it.m_cur_upper_bound - it.m_cur_base + 1, n, *it.m_params);
                compact_ranked_bitvector::decode(bv_enum, base, out);
                break;
            case indexed_sequence_t::index_type::third: {
                uint64_t offset =
                    it.m_sequences_offset + it.m_endpoint + type_bits;
                uint64_t mod = offset % alignment;
                uint64_t pad = 0;
                if (mod) {
                    pad = alignment - mod;
                }
                assert((offset + pad) % alignment == 0);
                offset += pad;
                uint8_t const* addr =
                    reinterpret_cast<uint8_t const*>((it.m_bv)->data().data()) +
                    offset / 8;
                uint64_t universe = it.m_cur_upper_bound - it.m_cur_base + 1;
                maskedvbyte_block::decode(addr, out, universe, n);
            } break;
            default:
                assert(false);
                __builtin_unreachable();
        }
        out += n;
    }
    return uint32_t(out - in);
}
}  // namespace pvb