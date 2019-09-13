#pragma once

#include "succinct/broadword.hpp"

namespace ds2i {

struct bits_enumerator {
    bits_enumerator(uint64_t const* data)
        : m_data(data)
        , m_pos(0)
        , m_buf(0)
        , m_avail(0) {}

    inline bool next() {
        if (!m_avail)
            fill_buf();
        bool b = m_buf & 1;
        m_buf >>= 1;
        --m_avail;
        ++m_pos;
        return b;
    }

    inline uint64_t take(size_t l) {
        if (m_avail < l)
            fill_buf();
        uint64_t val;
        if (l != 64) {
            val = m_buf & ((uint64_t(1) << l) - 1);
            m_buf >>= l;
        } else {
            val = m_buf;
        }
        m_avail -= l;
        m_pos += l;
        return val;
    }

    inline uint64_t skip_zeros() {
        uint64_t zs = 0;
        while (!m_buf) {
            m_pos += m_avail;
            zs += m_avail;
            m_avail = 0;
            fill_buf();
        }

        uint64_t l = succinct::broadword::lsb(m_buf);
        m_buf >>= l;
        m_buf >>= 1;
        m_avail -= l + 1;
        m_pos += l + 1;
        return zs + l;
    }

    inline uint64_t position() const {
        return m_pos;
    }

private:
    inline uint64_t get_word(uint64_t pos) const {
        uint64_t block = pos / 64;
        uint64_t shift = pos % 64;
        uint64_t word = m_data[block] >> shift;
        if (shift) {
            // XXX: does not check for last block
            word |= m_data[block + 1] << (64 - shift);
        }
        return word;
    }

    inline void fill_buf() {
        m_buf = get_word(m_pos);
        m_avail = 64;
    }

    uint64_t const* m_data;
    uint64_t m_pos;
    uint64_t m_buf;
    uint64_t m_avail;
};
}  // namespace ds2i
