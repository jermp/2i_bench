#pragma once

#include <stdexcept>

#include "util.hpp"
#include "block_sequence.hpp"
#include "compact_ranked_bitvector.hpp"

namespace pvb {

// template <typename T>
// struct is_byte_aligned {
//     static const bool value = false;
// };

// template <typename B>
// struct is_byte_aligned<block_sequence<B>> {
//     static const bool value = true;
// };

template <typename Encoder>
struct indexed_sequence {
    enum index_type { third = 0, ranked_bitvector = 1 };

    static const uint64_t type_bits = 1;

    static int type(succinct::bit_vector const& bv, uint64_t offset,
                    uint64_t /*universe*/, uint64_t /*n*/,
                    ds2i::global_parameters const* /*params*/) {
        return index_type(bv.get_word56(offset) &
                          ((uint64_t(1) << type_bits) - 1));
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
            if (m_type == third /*and is_byte_aligned<Encoder>::value*/) {
                uint64_t mod = (offset + type_bits) % alignment;
                if (mod) {
                    pad = alignment - mod;
                }
                assert((offset + type_bits + pad) % alignment == 0);
            }

            switch (m_type) {
                case third: {
                    m_th_enumerator = typename Encoder::enumerator(
                        bv, offset + type_bits + pad, universe, n, params);
                    break;
                }
                case ranked_bitvector: {
                    m_rb_enumerator = compact_ranked_bitvector::enumerator(
                        bv, offset + type_bits, universe, n, params);
                    break;
                }
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
