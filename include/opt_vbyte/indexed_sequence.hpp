#pragma once

#include <stdexcept>

#include "util.hpp"
#include "block_sequence.hpp"
#include "compact_elias_fano.hpp"
#include "compact_ranked_bitvector.hpp"
#include "all_ones_sequence.hpp"

namespace pvb {

template <typename T>
struct is_byte_aligned {
    static const bool value = false;
};

template <typename B>
struct is_byte_aligned<block_sequence<B>> {
    static const bool value = true;
};

template <typename Encoder = compact_elias_fano>
struct indexed_sequence {
    enum index_type {
        third = 0,
        ranked_bitvector = 1,
        all_ones = 2,
        index_types = 3
    };

    static const uint64_t type_bits = 1;  // all_ones is implicit

    static DS2I_FLATTEN_FUNC uint64_t
    bitsize(ds2i::global_parameters const& params, uint64_t third_cost,
            uint64_t universe, uint64_t n) {
        uint64_t best_cost = third_cost;
        uint64_t rb_cost = compact_ranked_bitvector::bitsize(params, universe,
                                                             n) /*+ type_bits*/;
        if (rb_cost < best_cost) {
            best_cost = rb_cost;
        }
        return best_cost;
    }

    template <typename Iterator>
    static DS2I_FLATTEN_FUNC uint64_t
    bitsize(Iterator begin, ds2i::global_parameters const& params,
            uint64_t universe, uint64_t n) {
        uint64_t third_cost =
            Encoder::bitsize(begin, params, universe, n) /*+ type_bits*/;
        return bitsize(params, third_cost, universe, n);
    }

    static int type(succinct::bit_vector const& bv, uint64_t offset,
                    uint64_t /*universe*/, uint64_t /*n*/,
                    ds2i::global_parameters const* /*params*/) {
        return index_type(bv.get_word56(offset) &
                          ((uint64_t(1) << type_bits) - 1));
    }

    template <typename Iterator>
    static void write(succinct::bit_vector_builder& bvb, Iterator begin,
                      uint64_t universe, uint64_t n,
                      ds2i::global_parameters const& params) {
        uint64_t best_cost =
            Encoder::bitsize(begin, params, universe, n) + type_bits;
        int best_type = third;

        uint64_t rb_cost =
            compact_ranked_bitvector::bitsize(params, universe, n) + type_bits;
        if (rb_cost < best_cost) {
            best_cost = rb_cost;
            best_type = ranked_bitvector;
        }

        bvb.append_bits(best_type, type_bits);
        if (best_type == third and is_byte_aligned<Encoder>::value) {
            push_pad(bvb, alignment);
        }

        switch (best_type) {
            case third:
                Encoder::write(bvb, begin, universe, n, params);
                break;
            case ranked_bitvector:
                compact_ranked_bitvector::write(bvb, begin, universe, n,
                                                params);
                break;
            default:
                assert(false);
        }
    }

    class enumerator {
    public:
        typedef std::pair<uint64_t, uint64_t> value_type;  // (position, value)

        enumerator() {}

        enumerator(succinct::bit_vector const& bv, uint64_t offset,
                   uint64_t universe, uint64_t n,
                   ds2i::global_parameters const& params) {
            m_type = index_type(bv.get_word56(offset) &
                                ((uint64_t(1) << type_bits) - 1));

            uint64_t pad = 0;
            if (m_type == third and is_byte_aligned<Encoder>::value) {
                uint64_t mod = (offset + type_bits) % alignment;
                if (mod) {
                    pad = alignment - mod;
                }
                assert((offset + type_bits + pad) % alignment == 0);
            }

            switch (m_type) {
                case third:
                    m_th_enumerator = typename Encoder::enumerator(
                        bv, offset + type_bits + pad, universe, n, params);
                    break;
                case ranked_bitvector:
                    m_rb_enumerator = compact_ranked_bitvector::enumerator(
                        bv, offset + type_bits, universe, n, params);
                    break;
                default:
                    throw std::invalid_argument("Unsupported type");
            }
        }

#define ENUMERATOR_METHOD(RETURN_TYPE, METHOD, FORMALS, ACTUALS) \
    RETURN_TYPE DS2I_FLATTEN_FUNC METHOD FORMALS {               \
        switch (__builtin_expect(m_type, third)) {               \
            case third:                                          \
                return m_th_enumerator.METHOD ACTUALS;           \
            case ranked_bitvector:                               \
                return m_rb_enumerator.METHOD ACTUALS;           \
            default:                                             \
                assert(false);                                   \
                __builtin_unreachable();                         \
        }                                                        \
    }                                                            \
    /**/

        ENUMERATOR_METHOD(value_type, move, (uint64_t position), (position));
        ENUMERATOR_METHOD(value_type, next_geq, (uint64_t lower_bound),
                          (lower_bound));
        ENUMERATOR_METHOD(value_type, next, (), ());
        ENUMERATOR_METHOD(uint64_t, size, () const, ());
        ENUMERATOR_METHOD(uint64_t, prev_value, () const, ());

#undef ENUMERATOR_METHOD
#undef ENUMERATOR_VOID_METHOD

        index_type m_type;
        union {
            typename Encoder::enumerator m_th_enumerator;
            compact_ranked_bitvector::enumerator m_rb_enumerator;
        };
    };
};
}  // namespace pvb
