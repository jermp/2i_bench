#pragma once

#include <vector>
#include <cmath>
#include <cassert>
#include <numeric>

namespace ds2i {

struct gaps_entropy {
    gaps_entropy(uint32_t universe)
        : m_bits(0)
        , m_prev(0)
        , m_freqs(universe, 0) {}

    void eat(uint32_t val) {
        uint32_t gap = val;
        if (m_prev != 0) gap -= m_prev + 1;
        ++m_freqs[gap];
        m_prev = val;
    }

    void init() {
        m_prev = 0;
    }

    void finalize() {
        uint64_t total_postings =
            std::accumulate(m_freqs.begin(), m_freqs.end(), uint64_t(0));
        double entropy = 0.0;
        for (uint64_t i = 0; i != m_freqs.size(); ++i) {
            if (m_freqs[i]) {
                double p = static_cast<double>(m_freqs[i]) / total_postings;
                entropy += -p * std::log2(p);
            }
        }
        m_bits = entropy * total_postings;
    }

    size_t bits() const {
        return m_bits;
    }

private:
    size_t m_bits;
    uint32_t m_prev;
    std::vector<uint64_t> m_freqs;
};

struct gaps_logs {
    gaps_logs(uint32_t /*universe*/)
        : m_bits(0)
        , m_prev(0) {}

    void eat(uint32_t val) {
        assert(val > m_prev);
        uint32_t gap = val;
        if (m_prev != 0) gap -= m_prev + 1;
        m_bits += std::ceil(std::log2(gap + 1));
        m_prev = val;
    }

    void init() {
        m_prev = 0;
    }

    void finalize() {}

    size_t bits() const {
        return m_bits;
    }

private:
    size_t m_bits;
    uint32_t m_prev;
};

}  // namespace ds2i
