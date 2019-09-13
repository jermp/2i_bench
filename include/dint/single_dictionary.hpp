#pragma once

#include <unordered_map>
#include <fstream>

#include <succinct/mappable_vector.hpp>

#include "dint_configuration.hpp"
#include "hash_utils.hpp"
#include "util.hpp"
#include "dictionary_building_utils.hpp"

namespace ds2i {

template <uint32_t t_num_entries, uint32_t t_max_entry_size,
          typename CompactingPolicy>
struct single_dictionary {
    static_assert(is_power_of_two(t_max_entry_size), "");
    static const uint32_t num_entries = t_num_entries;
    static const uint32_t max_entry_size = t_max_entry_size;
    static const uint32_t invalid_index = uint32_t(-1);
    static const uint32_t reserved = EXCEPTIONS + 5;

    struct builder {
        static const uint32_t num_entries = single_dictionary::num_entries;
        static const uint32_t max_entry_size =
            single_dictionary::max_entry_size;
        static const uint32_t invalid_index = single_dictionary::invalid_index;
        static const uint32_t reserved = single_dictionary::reserved;

        // uint64_t codewords = 0;
        // uint64_t small_exceptions = 0;
        // uint64_t large_exceptions = 0;
        // uint64_t block_encoded_with_large_dict = 0;
        // uint64_t block_encoded_with_small_dict = 0;

        builder() : m_size(reserved) {}

        void init() {
            m_size = reserved;
            m_offsets.reserve(num_entries);

            // NOTE: push [max_entry_size] 0s at the beginning of the table
            // to be copied in case of a run, thus the offset corresponding to
            // the indexes 2, 3, 4, 5, 6 (i.e., runs) is always 0
            for (uint32_t i = 0; i != max_entry_size; ++i) {
                m_table.push_back(0);
            }
            for (uint32_t i = 0; i != EXCEPTIONS; ++i) {
                m_offsets.push_back(0);
            }
            for (uint32_t i = 0, size = 256; i != 5; ++i, size /= 2) {
                uint32_t size_and_offset = (size - 1) << 24;  // offset is 0
                m_offsets.push_back(size_and_offset);
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
            uint32_t offsets_size = m_offsets.size();
            uint32_t table_size = m_table.size();
            dictionary_file.write(reinterpret_cast<char const*>(&m_size),
                                  sizeof(uint32_t));
            dictionary_file.write(reinterpret_cast<char const*>(&offsets_size),
                                  sizeof(uint32_t));
            dictionary_file.write(reinterpret_cast<char const*>(&table_size),
                                  sizeof(uint32_t));
            dictionary_file.write(
                reinterpret_cast<char const*>(m_offsets.data()),
                offsets_size * sizeof(uint32_t));
            dictionary_file.write(reinterpret_cast<char const*>(m_table.data()),
                                  table_size * sizeof(uint32_t));
        }

        size_t load(std::ifstream& dictionary_file) {
            uint32_t offsets_size = 0;
            uint32_t table_size = 0;
            size_t read_bytes = 0;
            dictionary_file.read(reinterpret_cast<char*>(&m_size),
                                 sizeof(uint32_t));
            dictionary_file.read(reinterpret_cast<char*>(&offsets_size),
                                 sizeof(uint32_t));
            dictionary_file.read(reinterpret_cast<char*>(&table_size),
                                 sizeof(uint32_t));
            read_bytes += 3 * sizeof(uint32_t);
            m_table.resize(table_size);
            m_offsets.resize(offsets_size);
            dictionary_file.read(reinterpret_cast<char*>(m_offsets.data()),
                                 offsets_size * sizeof(uint32_t));
            dictionary_file.read(reinterpret_cast<char*>(m_table.data()),
                                 table_size * sizeof(uint32_t));
            read_bytes += (offsets_size + table_size) * sizeof(uint32_t);
            return read_bytes;
        }

        bool full() {
            return m_size == num_entries;
        }

        bool append(uint32_t const* entry, uint32_t entry_size,
                    uint32_t /*dictionary_id*/) {
            assert(entry_size > 0 and entry_size <= max_entry_size);
            if (full()) {
                return false;
            }

            m_targets.emplace_back(entry, entry + entry_size);
            ++m_size;
            return true;
        }

        void build() {
            {
                std::vector<std::vector<target_t>> targets;
                targets.push_back(m_targets);
                logger() << "compacting..." << std::endl;
                auto compacted_targets = CompactingPolicy::compact(targets);
                logger() << "creating table..." << std::endl;
                for (auto& cur : compacted_targets) {
                    std::copy(cur.entry.begin(), cur.entry.end(),
                              std::back_inserter(m_table));
                }
            }

            {
                logger() << "creating offsets..." << std::endl;
                for (auto& cur : m_targets) {
                    auto& entry = cur.entry;
                    auto found_itr = std::search(m_table.begin(), m_table.end(),
                                                 entry.begin(), entry.end());
                    assert(found_itr != m_table.end());
                    uint32_t entry_size = entry.size();
                    uint32_t offset = std::distance(m_table.begin(), found_itr);
                    uint32_t size_and_offset =
                        ((entry_size - 1) << 24) | offset;
                    m_offsets.push_back(size_and_offset);
                }
            }
        }

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

        void build(single_dictionary& dict) {
            dict.m_offsets.steal(m_offsets);
            dict.m_table.steal(m_table);
            builder().swap(*this);
        }

        void swap(builder& other) {
            std::swap(m_size, other.m_size);
            m_offsets.swap(other.m_offsets);
            m_table.swap(other.m_table);
            m_map.swap(other.m_map);
        }

        uint32_t size() const {
            return m_size;
        }

        static std::string type() {
            return std::string("single_") + CompactingPolicy::type();
        }

        // print vocabulary entries usage
        void print_usage() {
            // TODO
        }

        uint32_t size(uint32_t i) const {
            assert(i < size());
            return (m_offsets[i] >> 24) + 1;
        }

        uint32_t offset(uint32_t i) const {
            assert(i < size());
            return m_offsets[i] & 0xFFFFFF;
        }

        uint32_t const* get(uint32_t i) const {
            return &m_table[offset(i)];
        }

    private:
        std::vector<target_t> m_targets;

        uint32_t m_size;
        std::vector<uint32_t> m_offsets;
        std::vector<uint32_t> m_table;

        // map from hash codes to table indexes, used during encoding
        std::unordered_map<uint64_t, uint32_t> m_map;
    };

    single_dictionary() {}

    uint32_t copy(uint32_t i, uint32_t* out) const {
        assert(i < num_entries);
        uint32_t size_and_offset = m_offsets[i];
        uint32_t offset = size_and_offset & 0xFFFFFF;
        uint32_t size = (size_and_offset >> 24) + 1;
        uint32_t const* ptr = &m_table[offset];
        memcpy(out, ptr, max_entry_size * sizeof(uint32_t));
        return size;
    }

    void swap(single_dictionary& other) {
        m_offsets.swap(other.m_offsets);
        m_table.swap(other.m_table);
    }

    template <typename Visitor>
    void map(Visitor& visit) {
        visit(m_offsets, "m_offsets")(m_table, "m_table");
    }

private:
    succinct::mapper::mappable_vector<uint32_t> m_offsets;
    succinct::mapper::mappable_vector<uint32_t> m_table;
};

}  // namespace ds2i
