#pragma once

#include "util.hpp"
#include "dint_configuration.hpp"
#include "statistics_collectors.hpp"

namespace ds2i {

struct dint_block {
    static const uint64_t block_size = constants::block_size;
    static const uint64_t overflow = 256;

    template <typename Dictionary>
    static uint8_t const* decode(Dictionary const& dict, uint8_t const* in,
                                 uint32_t* out, uint32_t sum_of_values,
                                 size_t n) {
        if (DS2I_UNLIKELY(n < block_size)) {
            return interpolative_block::decode(in, out, sum_of_values, n);
        }

        uint16_t const* ptr = reinterpret_cast<uint16_t const*>(in);
        // uint8_t const* ptr = in; // if b = 8
        for (size_t i = 0; i != n; ++ptr) {
            uint32_t index = *ptr;
            uint32_t decoded_ints = 1;
            if (DS2I_LIKELY(index > EXCEPTIONS - 1)) {
                decoded_ints = dict.copy(index, out);
            } else {
                if (index == 1) {  // 4-byte exception
                    *out = *(reinterpret_cast<uint32_t const*>(++ptr));
                    ++ptr;
                    // if b = 8
                    // *out = *(reinterpret_cast<uint32_t const*>(++ptr));
                    // ptr += 3;
                } else {  // 2-byte exception
                    *out = *(++ptr);
                    // if b = 8
                    // *out = *(reinterpret_cast<uint16_t const*>(++ptr));
                    // ptr += 1;
                }
            }
            out += decoded_ints;
            i += decoded_ints;
        }

        return reinterpret_cast<uint8_t const*>(ptr);
        // if b = 8
        // return ptr;
    }
};

struct greedy_dint_single_dict_block {
    static const uint64_t block_size = constants::block_size;
    static const uint64_t overflow = dint_block::overflow;

    template <typename Builder>
    static void encode(Builder& builder, uint32_t const* in,
                       uint32_t sum_of_values, uint32_t n,
                       std::vector<uint8_t>& out) {
        if (n < block_size) {
            interpolative_block::encode(in, sum_of_values, n, out);
            return;
        }

        uint32_t const* begin = in;
        uint32_t const* end = begin + n;

        while (begin < end) {
            uint32_t longest_run_size = 0;
            uint32_t run_size = std::min<uint64_t>(256, end - begin);
            uint32_t index = EXCEPTIONS;

            for (uint32_t const* ptr = begin; ptr != begin + run_size; ++ptr) {
                if (*ptr == 0) {
                    ++longest_run_size;
                } else {
                    break;
                }
            }

            if (longest_run_size >= 16) {
                uint32_t k = 256;
                while (longest_run_size < k and k > 16) {
                    ++index;
                    k /= 2;
                }
                write_index(index, out);
                // ++builder.codewords;
                begin += k;
            } else {
                for (uint32_t s = 0; s < constants::num_target_sizes; ++s) {
                    uint32_t sub_block_size = constants::target_sizes[s];
                    uint32_t len =
                        std::min<uint32_t>(sub_block_size, end - begin);
                    index = builder.lookup(begin, len);
                    if (index != Builder::invalid_index) {
                        write_index(index, out);
                        // ++builder.codewords;
                        begin += len;
                        break;
                    }
                }

                if (index == Builder::invalid_index) {
                    uint32_t exception = *begin;
                    auto ptr = reinterpret_cast<uint8_t const*>(&exception);
                    if (exception < 65536) {
                        // ++builder.small_exceptions;
                        out.insert(out.end(), 0);
                        out.insert(out.end(), 0);  // comment if b = 8
                        out.insert(out.end(), ptr, ptr + 2);

                    } else {
                        // ++builder.large_exceptions;
                        out.insert(out.end(), 1);
                        out.insert(out.end(), 0);  // comment if b = 8
                        out.insert(out.end(), ptr, ptr + 4);
                    }

                    begin += 1;
                }
            }
        }
    }

    template <typename Dictionary>
    static uint8_t const* decode(Dictionary const& dict, uint8_t const* in,
                                 uint32_t* out, uint32_t sum_of_values,
                                 size_t n) {
        return dint_block::decode(dict, in, out, sum_of_values, n);
    }

private:
    static void write_index(uint32_t index, std::vector<uint8_t>& out) {
        auto ptr = reinterpret_cast<uint8_t const*>(&index);
        out.insert(out.end(), ptr, ptr + 2);  // b = 16
        // out.insert(out.end(), ptr, ptr + 1); // b = 8
    }
};

struct opt_dint_single_dict_block {
    static const uint64_t block_size = constants::block_size;
    static const uint64_t overflow = dint_block::overflow;

    template <typename Builder>
    static void encode(Builder& builder, uint32_t const* begin, uint64_t n,
                       std::vector<uint8_t>& out, uint32_t b) {
        std::vector<node> path(n + 2);
        path[0] = {0, 1, 0};  // dummy node
        for (uint32_t i = 1; i < n + 1; ++i) {
            path[i] = {i - 1, 1, 3 * i};
        }

        for (uint32_t i = 0; i < n; ++i) {
            uint32_t longest_run_size = 0;
            uint32_t run_size = std::min<uint64_t>(256, n - i);
            uint32_t index = EXCEPTIONS;

            for (uint32_t j = i; j != i + run_size; ++j) {
                if (begin[j] == 0) {
                    ++longest_run_size;
                } else {
                    break;
                }
            }

            if (longest_run_size >= 16) {
                uint32_t k = 256;
                while (longest_run_size < k and k > 16) {
                    k /= 2;
                    ++index;
                }
                while (k >= 16) {
                    uint32_t c = path[i].cost + 1;
                    if (path[i + k].cost > c) {
                        path[i + k] = {i, index, c};
                    }

                    k /= 2;
                    ++index;
                }
            }

            for (uint32_t s = 0; s < constants::num_target_sizes; ++s) {
                uint32_t sub_block_size = constants::target_sizes[s];
                uint32_t len = std::min<uint32_t>(sub_block_size, n - i);
                index = builder.lookup(begin + i, len);
                if (index != Builder::invalid_index) {
                    uint32_t c = path[i].cost + 1;
                    if (path[i + len].cost > c) {
                        path[i + len] = {i, index, c};
                    }
                } else {
                    if (sub_block_size == 1) {  // exceptions
                        uint32_t exception = begin[i];
                        uint32_t c = path[i].cost + 2;  // small exception cost
                        index = 0;

                        if (exception > 65536 - 1) {
                            c += 1;  // large exception cost
                            index = 1;
                        }

                        if (path[i + 1].cost > c) {
                            path[i + 1] = {i, index, c};
                        }
                    }
                }
            }
        }

        std::vector<node> encoding;
        uint32_t i = n;
        while (i != 0) {
            uint32_t parent = path[i].parent;
            encoding.push_back(path[i]);
            i = parent;
        }

        std::reverse(encoding.begin(), encoding.end());
        encoding.emplace_back(n, 1, -1);  // final dummy node

        uint32_t pos = 0;
        for (uint32_t i = 0; i < encoding.size() - 1; ++i) {
            uint32_t index = encoding[i].codeword;
            uint32_t len = encoding[i + 1].parent - encoding[i].parent;

            if (index > 1) {
                // ++builder.codewords;
                write_index(index, out, b);
            } else {
                assert(len == 1);
                uint32_t exception = begin[pos];
                auto ptr = reinterpret_cast<uint8_t const*>(&exception);
                if (index == 0) {
                    // ++builder.small_exceptions;
                    out.insert(out.end(), 0);
                    if (b == 16) {
                        out.insert(out.end(), 0);
                    }
                    out.insert(out.end(), ptr, ptr + 2);
                } else {
                    // ++builder.large_exceptions;
                    out.insert(out.end(), 1);
                    if (b == 16) {
                        out.insert(out.end(), 0);
                    }
                    out.insert(out.end(), ptr, ptr + 4);
                }
            }
            pos += len;
        }

        assert(pos == n);
    }

    template <typename Builder>
    static void encode(Builder& builder, uint32_t const* in,
                       uint32_t sum_of_values, uint32_t n,
                       std::vector<uint8_t>& out) {
        if (n < block_size) {
            interpolative_block::encode(in, sum_of_values, n, out);
            return;
        }

        encode(builder, in, n, out, constants::log2_num_entries);
    }

    template <typename Dictionary>
    static uint8_t const* decode(Dictionary const& dict, uint8_t const* in,
                                 uint32_t* out, uint32_t sum_of_values,
                                 size_t n) {
        return dint_block::decode(dict, in, out, sum_of_values, n);
    }

private:
    static void write_index(uint32_t index, std::vector<uint8_t>& out,
                            uint32_t b) {
        auto ptr = reinterpret_cast<uint8_t const*>(&index);
        assert(b == 8 or b == 16);
        out.insert(out.end(), ptr, ptr + b / 8);
    }
};

struct opt_dint_multi_dict_block {
    static const uint64_t block_size = constants::block_size;
    static const uint64_t overflow = dint_block::overflow;

    template <typename Builder>
    static void encode(Builder& builder, uint32_t dictionary_id,
                       uint32_t const* begin, uint64_t n,
                       std::vector<uint8_t>& out, uint32_t b) {
        std::vector<node> path(n + 2);
        path[0] = {0, 1, 0};  // dummy node
        for (uint32_t i = 1; i < n + 1; ++i) {
            path[i] = {i - 1, 1, 3 * i};
        }

        for (uint32_t i = 0; i < n; ++i) {
            uint32_t longest_run_size = 0;
            uint32_t run_size = std::min<uint64_t>(256, n - i);
            uint32_t index = EXCEPTIONS;

            for (uint32_t j = i; j != i + run_size; ++j) {
                if (begin[j] == 0) {
                    ++longest_run_size;
                } else {
                    break;
                }
            }

            if (longest_run_size >= 16) {
                uint32_t k = 256;
                while (longest_run_size < k and k > 16) {
                    k /= 2;
                    ++index;
                }
                while (k >= 16) {
                    uint32_t c = path[i].cost + 1;
                    if (path[i + k].cost > c) {
                        path[i + k] = {i, index, c};
                    }

                    k /= 2;
                    ++index;
                }
            }

            for (uint32_t s = 0; s < constants::num_target_sizes; ++s) {
                uint32_t sub_block_size = constants::target_sizes[s];
                uint32_t len = std::min<uint32_t>(sub_block_size, n - i);
                index = builder.lookup(dictionary_id, begin + i, len, b);
                if (index != Builder::invalid_index) {
                    uint32_t c = path[i].cost + 1;
                    if (path[i + len].cost > c) {
                        path[i + len] = {i, index, c};
                    }
                } else {
                    if (sub_block_size == 1) {  // exceptions
                        uint32_t exception = begin[i];
                        uint32_t c = path[i].cost + 2;  // small exception cost
                        index = 0;

                        if (exception > 65536 - 1) {
                            c += 1;  // large exception cost
                            index = 1;
                        }

                        if (path[i + 1].cost > c) {
                            path[i + 1] = {i, index, c};
                        }
                    }
                }
            }
        }

        std::vector<node> encoding;
        uint32_t i = n;
        while (i != 0) {
            uint32_t parent = path[i].parent;
            encoding.push_back(path[i]);
            i = parent;
        }

        std::reverse(encoding.begin(), encoding.end());
        encoding.emplace_back(n, 1, -1);  // final dummy node

        uint32_t pos = 0;
        for (uint32_t i = 0; i < encoding.size() - 1; ++i) {
            uint32_t index = encoding[i].codeword;
            uint32_t len = encoding[i + 1].parent - encoding[i].parent;

            if (index > 1) {
                // ++builder.codewords;
                write_index(index, out, b);
            } else {
                assert(len == 1);
                uint32_t exception = begin[pos];
                auto ptr = reinterpret_cast<uint8_t const*>(&exception);
                if (index == 0) {
                    // ++builder.small_exceptions;
                    out.insert(out.end(), 0);
                    if (b == 16) {
                        out.insert(out.end(), 0);
                    }
                    out.insert(out.end(), ptr, ptr + 2);
                } else {
                    // ++builder.large_exceptions;
                    out.insert(out.end(), 1);
                    if (b == 16) {
                        out.insert(out.end(), 0);
                    }
                    out.insert(out.end(), ptr, ptr + 4);
                }
            }
            pos += len;
        }

        assert(pos == n);
    }

    template <typename Builder>
    static void encode(Builder& builder, uint32_t const* in,
                       uint32_t sum_of_values, uint32_t n,
                       std::vector<uint8_t>& out) {
        if (n < block_size) {
            interpolative_block::encode(in, sum_of_values, n, out);
            return;
        }

        // Option (1): choose the best dictionary
        std::vector<std::vector<uint8_t>> encoded(2 * constants::num_selectors);
        size_t best_size = size_t(-1);
        uint32_t selector_code = 0;
        for (uint32_t s = 0; s != constants::num_selectors; ++s) {
            encode(builder, s, in, n, encoded[s], 16);
            encode(builder, s, in, n, encoded[s + constants::num_selectors], 8);
            size_t smallest_size = encoded[s].size();
            uint32_t sc = s;
            if (encoded[s + constants::num_selectors].size() <= smallest_size) {
                smallest_size = encoded[s + constants::num_selectors].size();
                sc += constants::num_selectors;
            }
            if (smallest_size < best_size) {
                best_size = smallest_size;
                selector_code = sc;
            }
        }
        // control byte
        out.push_back(selector_code);
        out.insert(out.end(), encoded[selector_code].begin(),
                   encoded[selector_code].end());

        // // Option (2): select the dictionary based on the context
        // selector sct;
        // uint32_t selector_code = sct.get(in, n);
        // std::vector<std::vector<uint8_t>> encoded(2);
        // encode(builder, selector_code, in, n, encoded[0], 16);
        // // also b = 8 is used
        // encode(builder, selector_code, in, n, encoded[1],  8);
        // size_t smallest_size = encoded[0].size();
        // if (encoded[1].size() <= smallest_size) {
        //     selector_code += constants::num_selectors;
        // }

        // // control byte
        // out.push_back(selector_code);
        // out.insert(out.end(), encoded[selector_code >=
        // constants::num_selectors].begin(),
        //                       encoded[selector_code >=
        //                       constants::num_selectors].end());

        // Option (3): select the LARGE dictionary (only) based on the context
        // selector sct;
        // uint32_t selector_code = sct.get(in, n);
        // out.push_back(selector_code);
        // encode(builder, selector_code, in, n, out, 16);
    }

    template <typename MultiDictionary>
    static uint8_t const* decode(MultiDictionary const& multi_dict,
                                 uint8_t const* in, uint32_t* out,
                                 uint32_t sum_of_values, size_t n) {
        if (DS2I_UNLIKELY(n < block_size)) {
            return interpolative_block::decode(in, out, sum_of_values, n);
        }

        uint8_t selector_code = *in;
        if (selector_code < constants::num_selectors) {
            uint16_t const* ptr = reinterpret_cast<uint16_t const*>(in + 1);
            for (size_t i = 0; i != n; ++ptr) {
                uint32_t index = *ptr;
                uint32_t decoded_ints = 1;
                if (DS2I_LIKELY(index > EXCEPTIONS - 1)) {
                    decoded_ints = multi_dict.copy(selector_code, index, out);
                } else {
                    if (index == 1) {  // 4-byte exception
                        *out = *(reinterpret_cast<uint32_t const*>(++ptr));
                        ++ptr;
                    } else {  // 2-byte exception
                        *out = *(++ptr);
                    }
                }
                out += decoded_ints;
                i += decoded_ints;
            }
            return reinterpret_cast<uint8_t const*>(ptr);
        } else {
            selector_code -= constants::num_selectors;
            uint8_t const* ptr = in + 1;
            for (size_t i = 0; i != n; ++ptr) {
                uint32_t index = *ptr;
                uint32_t decoded_ints = 1;
                if (DS2I_LIKELY(index > EXCEPTIONS - 1)) {
                    decoded_ints = multi_dict.copy(selector_code, index, out);
                } else {
                    if (index == 1) {  // 4-byte exception
                        *out = *(reinterpret_cast<uint32_t const*>(++ptr));
                        ptr += 3;
                    } else {  // 2-byte exception
                        *out = *(reinterpret_cast<uint16_t const*>(++ptr));
                        ptr += 1;
                    }
                }
                out += decoded_ints;
                i += decoded_ints;
            }
            return ptr;
        }
    }

private:
    static void write_index(uint32_t index, std::vector<uint8_t>& out,
                            uint32_t b) {
        auto ptr = reinterpret_cast<uint8_t const*>(&index);
        assert(b == 8 or b == 16);
        out.insert(out.end(), ptr, ptr + b / 8);
    }
};

}  // namespace ds2i
