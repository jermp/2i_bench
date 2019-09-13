#pragma once

#include <boost/iostreams/device/mapped_file.hpp>
#include <stdexcept>
#include <iterator>
#include <stdint.h>
#include <sys/mman.h>

#include "util.hpp"

namespace ds2i {

struct binary_blocks_collection {
    binary_blocks_collection(const char* filename) {
        m_file.open(filename);
        if (!m_file.is_open()) {
            throw std::runtime_error("Error opening file");
        }
        m_data = (uint32_t const*)m_file.data();
        m_data_size = m_file.size() / sizeof(m_data[0]);
        auto ret =
            posix_madvise((void*)m_data, m_data_size, POSIX_MADV_SEQUENTIAL);
        if (ret) {
            std::cerr << "Error calling madvice: " << errno;
        }

        // parse header
        m_num_blocks = *m_data;
        if (!m_num_blocks) {
            throw std::runtime_error("Number of blocks must not be 0");
        }
    }

    struct iterator;

    iterator begin() const {
        return iterator(this, 1);
    }

    iterator end() const {
        return iterator(this, m_data_size);
    }

    struct block {
        block()
            : m_freq(0)
            , m_begin(nullptr)
            , m_end(nullptr) {}

        uint32_t const* begin() const {
            return m_begin;
        }

        uint32_t const* end() const {
            return m_end;
        }

        uint32_t freq() const {
            return m_freq;
        }

        size_t size() const {
            return m_end - m_begin;
        }

    private:
        friend struct binary_blocks_collection::iterator;

        block(uint32_t freq, uint32_t const* begin, uint32_t const* end)
            : m_freq(freq)
            , m_begin(begin)
            , m_end(end) {}

        uint32_t m_freq;
        uint32_t const* m_begin;
        uint32_t const* m_end;
    };

    struct iterator {
        iterator()
            : m_collection(nullptr) {}

        block const& operator*() const {
            return m_cur_block;
        }

        block const* operator->() const {
            return &m_cur_block;
        }

        iterator& operator++() {
            m_pos = m_next_pos;
            read();
            return *this;
        }

        bool operator==(iterator const& other) const {
            assert(m_collection == other.m_collection);
            return m_pos == other.m_pos;
        }

        bool operator!=(iterator const& other) const {
            return !(*this == other);
        }

    private:
        friend struct binary_blocks_collection;

        iterator(binary_blocks_collection const* coll, uint64_t pos)
            : m_collection(coll)
            , m_pos(pos) {
            read();
        }

        void read() {
            assert(m_pos <= m_collection->m_data_size);
            if (m_pos == m_collection->m_data_size)
                return;

            size_t n = 0;
            size_t pos = m_pos;
            while (!(n = m_collection->m_data[pos++]))
                ;  // skip empty seqs
            // file might be truncated
            n = std::min(n, size_t(m_collection->m_data_size - pos));

            uint32_t freq = m_collection->m_data[pos];
            uint32_t const* begin = &m_collection->m_data[pos + 1];
            uint32_t const* end = begin + n;

            m_next_pos = pos + n + 1;
            m_cur_block = block(freq, begin, end);
        }

        binary_blocks_collection const* m_collection;
        size_t m_pos, m_next_pos;
        block m_cur_block;
    };

    uint64_t num_blocks() const {
        return m_num_blocks;
    }

private:
    boost::iostreams::mapped_file_source m_file;
    uint32_t const* m_data;
    size_t m_data_size;
    uint64_t m_num_blocks;
};
}  // namespace ds2i
