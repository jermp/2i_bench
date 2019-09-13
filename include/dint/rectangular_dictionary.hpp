#pragma once

#include <unordered_map>
#include <fstream>

#include <succinct/mappable_vector.hpp>

#include "dint_configuration.hpp"
#include "hash_utils.hpp"
#include "util.hpp"

namespace ds2i {

template <uint32_t t_num_entries, uint32_t t_max_entry_size>
struct rectangular_dictionary {
    static_assert(is_power_of_two(t_max_entry_size), "");
    static const uint32_t num_entries = t_num_entries;
    static const uint32_t max_entry_size = t_max_entry_size;
    static const uint32_t invalid_index = uint32_t(-1);
    static const uint32_t reserved = EXCEPTIONS + 5;

    struct builder {
        static const uint32_t num_entries = rectangular_dictionary::num_entries;
        static const uint32_t max_entry_size =
            rectangular_dictionary::max_entry_size;
        static const uint32_t invalid_index =
            rectangular_dictionary::invalid_index;
        static const uint32_t reserved = rectangular_dictionary::reserved;

        uint64_t codewords = 0;
        uint64_t small_exceptions = 0;
        uint64_t large_exceptions = 0;

        uint64_t block_encoded_with_large_dict = 0;
        uint64_t block_encoded_with_small_dict = 0;

        builder() : m_pos(0), m_size(reserved), m_table(0, 0) {}

        size_t num_bytes_for(uint64_t n) const {
            return n * (max_entry_size + 1) * sizeof(m_table.front());
        }

        void init() {
            m_pos = reserved * (max_entry_size + 1);
            m_size = reserved;
            m_table.resize(num_entries * (max_entry_size + 1), 0);

            uint32_t pos = max_entry_size + 1;
            for (int i = 0; i < EXCEPTIONS; ++i, pos += max_entry_size + 1) {
                m_table[pos - 1] = 1;
            }
            for (int i = 0, size = 256; i < 5;
                 ++i, pos += max_entry_size + 1, size /= 2) {
                m_table[pos - 1] = size;
            }
        }

        size_t load_from_file(std::string dict_file) {
            std::ifstream ifs(dict_file);
            return load(ifs);
        }

        bool try_store_to_file(std::string dict_file) const {
            std::ofstream ofs(dict_file);
            if (ofs) {
                write(ofs);
                return true;
            }
            return false;
        }

        void write(std::ofstream& dictionary_file) const {
            dictionary_file.write(reinterpret_cast<char const*>(&m_size),
                                  sizeof(uint32_t));
            dictionary_file.write(reinterpret_cast<char const*>(m_table.data()),
                                  num_bytes_for(m_size));
        }

        size_t load(std::ifstream& dictionary_file) {
            uint32_t size = 0;
            size_t read_bytes = 0;
            dictionary_file.read(reinterpret_cast<char*>(&size),
                                 sizeof(uint32_t));
            read_bytes += sizeof(uint32_t);
            init();
            m_size = size;
            size_t table_bytes = num_bytes_for(m_size);
            dictionary_file.read(reinterpret_cast<char*>(m_table.data()),
                                 table_bytes);
            read_bytes += table_bytes;
            return table_bytes;
        }

        bool full() {
            return m_size == num_entries;
        }

        bool append(uint32_t const* entry, uint32_t entry_size,
                    uint32_t /*dictionary_id*/) {
            if (full())
                return false;
            assert(m_pos % (max_entry_size + 1) == 0);
            std::copy(entry, entry + entry_size, &m_table[m_pos]);
            m_pos += max_entry_size + 1;
            m_table[m_pos - 1] = entry_size;
            ++m_size;
            return true;
        }

        void build() {}

        void prepare_for_encoding() {
            std::vector<uint32_t> run(256, 0);
            uint32_t i = EXCEPTIONS;
            for (uint32_t n = 256; n >= 16; n /= 2, ++i) {
                uint64_t hash = hash_bytes64(run.data(), n);
                m_map[hash] = i;
            }
            for (; i < size(); ++i) {
                uint64_t hash = hash_bytes64(get(i), size(i));
                m_map[hash] = i;
            }
        }

        uint32_t lookup(uint32_t const* begin, uint32_t entry_size) const {
            uint64_t hash = hash_bytes64(begin, entry_size);
            auto it = m_map.find(hash);
            if (it != m_map.end()) {
                assert((*it).second < num_entries);
                return (*it).second;
            }
            return invalid_index;
        }

        void build(rectangular_dictionary& dict) {
            dict.m_table.steal(m_table);
            builder().swap(*this);
        }

        void swap(builder& other) {
            std::swap(m_pos, other.m_pos);
            std::swap(m_size, other.m_size);
            m_table.swap(other.m_table);
            m_map.swap(other.m_map);
        }

        uint32_t size() const {
            return m_size;
        }

        static std::string type() {
            return "rectangular";
        }

        // print vocabulary entries usage
        void print_usage() {
            std::vector<uint32_t> sizes;
            sizes.push_back(EXCEPTIONS);
            for (uint32_t i = 0; i < constants::num_target_sizes; ++i) {
                sizes.push_back(0);
            }
            sizes.push_back(5);  // for the runs

            for (uint64_t i = reserved * (max_entry_size + 1);
                 i < m_table.size(); i += max_entry_size + 1) {
                uint32_t size = m_table[i + max_entry_size];
                uint32_t index = ceil_log2(size) + 1;
                assert(index < sizes.size());
                sizes[index] += 1;
            }

            std::cout << "rare: " << EXCEPTIONS << " ("
                      << EXCEPTIONS * 100.0 / num_entries << "%)" << std::endl;
            for (uint32_t i = 0; i < constants::num_target_sizes; ++i) {
                std::cout << "entries of size " << (uint32_t(1) << i) << ": "
                          << sizes[i + 1] << "("
                          << sizes[i + 1] * 100.0 / num_entries << "%)"
                          << std::endl;
            }
            std::cout << "freq.: 5 (" << 5 * 100.0 / num_entries << "%)"
                      << std::endl;
        }

        uint32_t size(uint32_t i) const {
            uint32_t begin = i * (t_max_entry_size + 1);
            uint32_t end = begin + t_max_entry_size;
            return m_table[end];
        }

        uint32_t const* get(uint32_t i) const {
            uint32_t begin = i * (t_max_entry_size + 1);
            return &m_table[begin];
        }

    private:
        uint32_t m_pos;
        uint32_t m_size;
        std::vector<uint32_t> m_table;

        // map from hash codes to table indexes, used during encoding
        std::unordered_map<uint64_t, uint32_t> m_map;
    };

    rectangular_dictionary() {}

    uint32_t copy(uint32_t i, uint32_t* out) const {
        assert(i < num_entries);
        uint32_t begin = i * (max_entry_size + 1);
        uint32_t const* ptr = &m_table[begin];
        memcpy(out, ptr, max_entry_size * sizeof(uint32_t));
        uint32_t size = *(ptr + max_entry_size);  // m_table[end];
        return size;
    }

    void swap(rectangular_dictionary& other) {
        m_table.swap(other.m_table);
    }

    template <typename Visitor>
    void map(Visitor& visit) {
        visit(m_table, "m_table");
    }

private:
    succinct::mapper::mappable_vector<uint32_t> m_table;
};
}  // namespace ds2i
