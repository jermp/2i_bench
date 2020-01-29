#pragma once

#include "succinct/util.hpp"
#include "block_codecs.hpp"

namespace pvb {

template <typename BlockCodec>
struct block_sequence {
    struct enumerator {
        enumerator() {}

        typedef std::pair<uint64_t, uint64_t> value_type;  // (position, value)

        enumerator(succinct::bit_vector const& bv, uint64_t offset,
                   uint64_t universe, uint64_t n,
                   ds2i::global_parameters const&)
            : m_n(n)
            , m_universe(universe)
            , m_pos_in_block(0)
            , m_value(0)
            , m_cur_block(0)
            , m_blocks(succinct::util::ceil_div(n, BlockCodec::block_size)) {
            assert(offset % alignment == 0);
            m_ptr =
                reinterpret_cast<uint8_t const*>(bv.data().data()) + offset / 8;
            decode_next_block();
            m_value = m_buffer[0] + 1;
        }

        void decode_next_block() {
            ++m_cur_block;
            uint64_t block_size =
                m_cur_block < m_blocks
                    ? BlockCodec::block_size
                    : m_n - (m_blocks - 1) * BlockCodec::block_size;
            m_ptr = BlockCodec::decode(m_ptr, m_buffer, m_universe, block_size);
            uint32_t last = m_cur_block > 1 ? m_value : uint32_t(-1);
            m_buffer[0] += last;
            m_pos_in_block = 0;
            m_value = 0;
        }

        value_type move(uint64_t pos) {
            assert(pos <= size());

            if (DS2I_UNLIKELY(pos == size())) {
                return value_type(size(), m_universe);
            }

            uint64_t block = pos / BlockCodec::block_size;
            if (DS2I_UNLIKELY(block != m_cur_block)) {
                while (m_cur_block < block) decode_next_block();
            }

            while (position() != pos) {
                m_value += m_buffer[++m_pos_in_block] + 1;
            }

            return value_type(position(), m_value);
        }

        value_type next_geq(uint64_t lower_bound) {
            while (m_value < lower_bound and position() < size()) {
                ++m_pos_in_block;
                if (m_pos_in_block == BlockCodec::block_size) {
                    decode_next_block();
                }
                m_value += m_buffer[m_pos_in_block] + 1;
            }
            if (DS2I_UNLIKELY(position() == size())) {
                m_value = m_universe;
            }
            return value_type(position(), m_value);
        }

        value_type next() {
            ++m_pos_in_block;
            if (DS2I_LIKELY(position() < size())) {
                if (m_pos_in_block == BlockCodec::block_size) {
                    decode_next_block();
                }
                m_value += m_buffer[m_pos_in_block] + 1;
            } else {
                m_value = m_universe;
            }
            return value_type(position(), m_value);
        }

        uint64_t position() const {
            return (m_cur_block - 1) * BlockCodec::block_size + m_pos_in_block;
        }

        uint64_t size() const {
            return m_n;
        }

    private:
        uint64_t m_n;
        uint64_t m_universe;
        uint32_t m_pos_in_block;
        uint32_t m_value;

        uint32_t m_cur_block;
        uint32_t m_blocks;
        uint8_t const* m_ptr;
        uint32_t m_buffer[BlockCodec::block_size];
    };
};

}  // namespace pvb
