#pragma once

#include "FastPFor/headers/VarIntG8IU.h"
#include "FastPFor/headers/optpfor.h"
#include "FastPFor/headers/variablebyte.h"
#include "FastPFor/headers/simple9.h"
#include "FastPFor/headers/simple16.h"
#include "FastPFor/headers/simple8b.h"

#include "streamvbyte/include/streamvbyte.h"
#include "MaskedVByte/include/varintencode.h"
#include "MaskedVByte/include/varintdecode.h"
#include "varintgb.h"
#include "interpolative_coding.hpp"

#include "qmx_codec.hpp"
#include "succinct/util.hpp"
#include "integer_codes.hpp"
#include "util.hpp"

namespace ds2i {

// workaround: VariableByte::decodeArray needs the buffer size, while we
// only know the number of values. It also pads to 32 bits. We need to
// rewrite
class TightVariableByte {
public:
    template <uint32_t i>
    static uint8_t extract7bits(const uint32_t val) {
        return static_cast<uint8_t>((val >> (7 * i)) & ((1U << 7) - 1));
    }

    template <uint32_t i>
    static uint8_t extract7bitsmaskless(const uint32_t val) {
        return static_cast<uint8_t>((val >> (7 * i)));
    }

    static void encode(const uint32_t* in, const size_t length, uint8_t* out,
                       size_t& nvalue) {
        uint8_t* bout = out;
        for (size_t k = 0; k < length; ++k) {
            const uint32_t val(in[k]);
            /**
             * Code below could be shorter. Whether it could be faster
             * depends on your compiler and machine.
             */
            if (val < (1U << 7)) {
                *bout = static_cast<uint8_t>(val | (1U << 7));
                ++bout;
            } else if (val < (1U << 14)) {
                *bout = extract7bits<0>(val);
                ++bout;
                *bout = extract7bitsmaskless<1>(val) | (1U << 7);
                ++bout;
            } else if (val < (1U << 21)) {
                *bout = extract7bits<0>(val);
                ++bout;
                *bout = extract7bits<1>(val);
                ++bout;
                *bout = extract7bitsmaskless<2>(val) | (1U << 7);
                ++bout;
            } else if (val < (1U << 28)) {
                *bout = extract7bits<0>(val);
                ++bout;
                *bout = extract7bits<1>(val);
                ++bout;
                *bout = extract7bits<2>(val);
                ++bout;
                *bout = extract7bitsmaskless<3>(val) | (1U << 7);
                ++bout;
            } else {
                *bout = extract7bits<0>(val);
                ++bout;
                *bout = extract7bits<1>(val);
                ++bout;
                *bout = extract7bits<2>(val);
                ++bout;
                *bout = extract7bits<3>(val);
                ++bout;
                *bout = extract7bitsmaskless<4>(val) | (1U << 7);
                ++bout;
            }
        }
        nvalue = bout - out;
    }

    static void encode_single(uint32_t val, std::vector<uint8_t>& out) {
        uint8_t buf[5];
        size_t nvalue;
        encode(&val, 1, buf, nvalue);
        out.insert(out.end(), buf, buf + nvalue);
    }

    static uint8_t const* decode(const uint8_t* in, uint32_t* out, size_t n) {
        const uint8_t* inbyte = in;
        for (size_t i = 0; i < n; ++i) {
            unsigned int shift = 0;
            for (uint32_t v = 0;; shift += 7) {
                uint8_t c = *inbyte++;
                v += ((c & 127) << shift);
                if ((c & 128)) {
                    *out++ = v;
                    break;
                }
            }
        }
        return inbyte;
    }
};

struct interpolative_block {
    static const uint64_t block_size = constants::block_size;
    static const uint64_t overflow = 0;

    static void encode(uint32_t const* in, uint32_t sum_of_values, size_t n,
                       std::vector<uint8_t>& out) {
        assert(n <= block_size);
        thread_local std::vector<uint32_t> inbuf(block_size);
        thread_local std::vector<uint32_t> outbuf;
        inbuf[0] = *in;
        for (size_t i = 1; i < n; ++i) {
            inbuf[i] = inbuf[i - 1] + in[i];
        }

        if (sum_of_values == uint32_t(-1)) {
            sum_of_values = inbuf[n - 1];
            TightVariableByte::encode_single(sum_of_values, out);
        }

        bit_writer bw(outbuf);
        bw.write_interpolative(inbuf.data(), n - 1, 0, sum_of_values);
        uint8_t const* bufptr = (uint8_t const*)outbuf.data();
        out.insert(out.end(), bufptr,
                   bufptr + succinct::util::ceil_div(bw.size(), 8));
    }

    static uint8_t const* DS2I_NOINLINE decode(uint8_t const* in, uint32_t* out,
                                               uint32_t sum_of_values,
                                               size_t n) {
        assert(n <= block_size);
        uint8_t const* inbuf = in;
        if (sum_of_values == uint32_t(-1)) {
            inbuf = TightVariableByte::decode(inbuf, &sum_of_values, 1);
        }

        out[n - 1] = sum_of_values;
        size_t read_interpolative = 0;
        if (n > 1) {
            bit_reader br((uint32_t const*)inbuf);
            br.read_interpolative(out, n - 1, 0, sum_of_values);
            for (size_t i = n - 1; i > 0; --i) {
                out[i] -= out[i - 1];
            }
            read_interpolative = succinct::util::ceil_div(br.position(), 8);
        }

        return inbuf + read_interpolative;
    }
};

struct optpfor_block {
    struct codec_type : FastPFor::OPTPFor<4, FastPFor::Simple16<false>> {
        uint8_t const* force_b;

        uint32_t findBestB(const uint32_t* in, uint32_t len) {
            // trick to force the choice of b from a parameter
            if (force_b) {
                return *force_b;
            }

            // this is mostly a cut&paste from FastPFor, but we stop the
            // optimization early as the b to test becomes larger than maxb
            uint32_t b = 0;
            uint32_t bsize = std::numeric_limits<uint32_t>::max();
            const uint32_t mb = FastPFor::maxbits(in, in + len);
            uint32_t i = 0;
            while (mb > 28 + possLogs[i])
                ++i;  // some schemes such as Simple16 don't code numbers
                      // greater than 28

            for (; i < possLogs.size(); i++) {
                if (possLogs[i] > mb && possLogs[i] >= mb) break;
                const uint32_t csize = tryB(possLogs[i], in, len);

                if (csize <= bsize) {
                    b = possLogs[i];
                    bsize = csize;
                }
            }
            return b;
        }
    };

    static const uint64_t block_size = codec_type::BlockSize;
    static const uint64_t overflow = 0;

    static void encode(uint32_t const* in, uint32_t sum_of_values, size_t n,
                       std::vector<uint8_t>& out,
                       uint8_t const* b = nullptr)  // if non-null forces b
    {
        thread_local codec_type optpfor_codec;
        thread_local std::vector<uint8_t> buf(2 * 4 * block_size);
        assert(n <= block_size);

        if (n < block_size) {
            interpolative_block::encode(in, sum_of_values, n, out);
            return;
        }

        size_t out_len = buf.size();

        optpfor_codec.force_b = b;
        optpfor_codec.encodeBlock(in, reinterpret_cast<uint32_t*>(buf.data()),
                                  out_len);
        out_len *= 4;
        out.insert(out.end(), buf.data(), buf.data() + out_len);
    }

    static uint8_t const* DS2I_NOINLINE decode(uint8_t const* in, uint32_t* out,
                                               uint32_t sum_of_values,
                                               size_t n) {
        thread_local codec_type
            optpfor_codec;  // pfor decoding is *not* thread-safe
        assert(n <= block_size);

        if (DS2I_UNLIKELY(n < block_size)) {
            return interpolative_block::decode(in, out, sum_of_values, n);
        }

        size_t out_len = block_size;
        uint8_t const* ret;

        ret = reinterpret_cast<uint8_t const*>(optpfor_codec.decodeBlock(
            reinterpret_cast<uint32_t const*>(in), out, out_len));
        assert(out_len == n);
        return ret;
    }
};

struct varint_G8IU_block {
    static const uint64_t block_size = constants::block_size;
    static const uint64_t overflow = 0;

    struct codec_type : FastPFor::VarIntG8IU {
        // rewritten version of decodeBlock optimized for when the output
        // size is known rather than the input
        // the buffers pointed by src and dst must be respectively at least
        // 9 and 8 elements large
        uint32_t decodeBlock(uint8_t const*& src, uint32_t* dst) const {
            uint8_t desc = *src;
            src += 1;
            const __m128i data =
                _mm_lddqu_si128(reinterpret_cast<__m128i const*>(src));
            src += 8;
            const __m128i result = _mm_shuffle_epi8(data, vecmask[desc][0]);
            _mm_storeu_si128(reinterpret_cast<__m128i*>(dst), result);
            int readSize = maskOutputSize[desc];

            if (readSize > 4) {
                const __m128i result2 = _mm_shuffle_epi8(
                    data,
                    vecmask[desc][1]);  //__builtin_ia32_pshufb128(data, shf2);
                _mm_storeu_si128(
                    reinterpret_cast<__m128i*>(dst + 4),
                    result2);  //__builtin_ia32_storedqu(dst + (16), result2);
            }

            return readSize;
        }
    };

    static void encode(uint32_t const* in, uint32_t sum_of_values, size_t n,
                       std::vector<uint8_t>& out) {
        thread_local codec_type varint_codec;
        thread_local std::vector<uint8_t> buf(2 * 4 * block_size);
        assert(n <= block_size);

        if (n < block_size) {
            interpolative_block::encode(in, sum_of_values, n, out);
            return;
        }

        size_t out_len = buf.size();

        const uint32_t* src = in;
        unsigned char* dst = buf.data();
        size_t srclen = n * 4;
        size_t dstlen = out_len;
        out_len = 0;
        while (srclen > 0 && dstlen >= 9) {
            out_len += varint_codec.encodeBlock(src, srclen, dst, dstlen);
        }
        assert(srclen == 0);
        out.insert(out.end(), buf.data(), buf.data() + out_len);
    }

    // we only allow varint to be inlined (others have DS2I_NOILINE)
    static uint8_t const* decode(uint8_t const* in, uint32_t* out,
                                 uint32_t sum_of_values, size_t n) {
        static codec_type varint_codec;  // decodeBlock is thread-safe
        assert(n <= block_size);

        if (DS2I_UNLIKELY(n < block_size)) {
            return interpolative_block::decode(in, out, sum_of_values, n);
        }

        size_t out_len = 0;
        uint8_t const* src = in;
        uint32_t* dst = out;
        while (out_len <= (n - 8)) {
            out_len += varint_codec.decodeBlock(src, dst + out_len);
        }

        // decodeBlock can overshoot, so we decode the last blocks in a
        // local buffer
        while (out_len < n) {
            uint32_t buf[8];
            size_t read = varint_codec.decodeBlock(src, buf);
            size_t needed = std::min(read, n - out_len);
            memcpy(dst + out_len, buf, needed * 4);
            out_len += needed;
        }
        assert(out_len == n);
        return src;
    }
};

struct qmx_block {
    static const uint64_t block_size = constants::block_size;
    static const uint64_t overflow = 512;

    static void encode(uint32_t const* in, uint32_t sum_of_values, size_t n,
                       std::vector<uint8_t>& out) {
        assert(n <= block_size);
        if (n < block_size) {
            interpolative_block::encode(in, sum_of_values, n, out);
            return;
        }
        thread_local QMX::codec<block_size> qmx_codec;
        thread_local std::vector<uint8_t> buf((overflow * 4) +
                                              2 * 4 * block_size);
        size_t out_len = buf.size();
        out_len = qmx_codec.encode(buf.data(), in);
        TightVariableByte::encode_single(out_len, out);
        out.insert(out.end(), buf.data(), buf.data() + out_len);
    }

    // we only allow varint to be inlined (others have DS2I_NOILINE)
    static uint8_t const* decode(uint8_t const* in, uint32_t* out,
                                 uint32_t sum_of_values, size_t n) {
        static QMX::codec<block_size> qmx_codec;  // decodeBlock is thread-safe
        assert(n <= block_size);

        if (DS2I_UNLIKELY(n < block_size)) {
            return interpolative_block::decode(in, out, sum_of_values, n);
        }

        uint32_t enc_len = 0;
        in = TightVariableByte::decode(in, &enc_len, 1);
        qmx_codec.decode(out, in, enc_len);
        return in + enc_len;
    }
};

struct vbyte_block {
    static const uint64_t block_size = constants::block_size;
    static const uint64_t overflow = 0;

    static void encode(uint32_t const* in, uint32_t /* sum_of_values */,
                       size_t n, std::vector<uint8_t>& out) {
        std::vector<uint8_t> buf(2 * 4 * block_size);
        size_t out_len = buf.size();
        TightVariableByte::encode(in, n, buf.data(), out_len);
        out.insert(out.end(), buf.data(), buf.data() + out_len);
    }

    static uint8_t const* decode(uint8_t const* in, uint32_t* out,
                                 uint32_t /* sum_of_values */, size_t n) {
        return TightVariableByte::decode(in, out, n);
    }
};

struct simple16_block {
    static const uint64_t block_size = constants::block_size;
    static const uint64_t overflow = 512;
    using codec_type = FastPFor::Simple16<false>;

    static void encode(uint32_t const* in, uint32_t /* sum_of_values */,
                       size_t n, std::vector<uint8_t>& out) {
        thread_local codec_type simple16_codec;
        assert(n <= block_size);
        // XXX this could be threadlocal static
        std::vector<uint8_t> buf(2 * 8 * block_size);
        size_t out_len = buf.size();
        simple16_codec.encodeArray(
            in, n, reinterpret_cast<uint32_t*>(buf.data()), out_len);
        out_len *= 4;
        out.insert(out.end(), buf.data(), buf.data() + out_len);
    }

    static uint8_t const* decode(uint8_t const* in, uint32_t* out,
                                 uint32_t /* sum_of_values */, size_t n) {
        assert(n <= block_size);
        uint8_t const* ret;
        thread_local codec_type simple16_codec;
        ret = reinterpret_cast<uint8_t const*>(simple16_codec.decodeArray(
            reinterpret_cast<uint32_t const*>(in), 1, out, n));
        return ret;
    }
};

struct simple9_block {
    static const uint64_t block_size = constants::block_size;
    static const uint64_t overflow = 512;
    using codec_type = FastPFor::Simple9<false>;

    static void encode(uint32_t const* in, uint32_t /* sum_of_values */,
                       size_t n, std::vector<uint8_t>& out) {
        thread_local codec_type codec;
        assert(n <= block_size);
        // XXX this could be threadlocal static
        std::vector<uint8_t> buf(2 * 8 * block_size);
        size_t out_len = buf.size();
        codec.encodeArray(in, n, reinterpret_cast<uint32_t*>(buf.data()),
                          out_len);
        out_len *= 4;
        out.insert(out.end(), buf.data(), buf.data() + out_len);
    }

    static uint8_t const* decode(uint8_t const* in, uint32_t* out,
                                 uint32_t /* sum_of_values */, size_t n) {
        assert(n <= block_size);
        uint8_t const* ret;
        thread_local codec_type codec;
        ret = reinterpret_cast<uint8_t const*>(codec.decodeArray(
            reinterpret_cast<uint32_t const*>(in), 1, out, n));
        return ret;
    }
};

struct simple8b_block {
    static const uint64_t block_size = constants::block_size;
    static const uint64_t overflow = 512;
    using codec_type = FastPFor::Simple8b<false>;

    static void encode(uint32_t const* in, uint32_t /* sum_of_values */,
                       size_t n, std::vector<uint8_t>& out) {
        thread_local codec_type codec;
        assert(n <= block_size);
        // XXX this could be threadlocal static
        std::vector<uint8_t> buf(2 * 8 * block_size);
        size_t out_len = buf.size();
        codec.encodeArray(in, n, reinterpret_cast<uint32_t*>(buf.data()),
                          out_len);
        out_len *= 4;
        out.insert(out.end(), buf.data(), buf.data() + out_len);
    }

    static uint8_t const* decode(uint8_t const* in, uint32_t* out,
                                 uint32_t /* sum_of_values */, size_t n) {
        assert(n <= block_size);
        uint8_t const* ret;
        thread_local codec_type codec;
        ret = reinterpret_cast<uint8_t const*>(codec.decodeArray(
            reinterpret_cast<uint32_t const*>(in), 1, out, n));
        return ret;
    }
};

struct streamvbyte_block {
    static const uint64_t block_size = constants::block_size;
    static const uint64_t overflow = 512;

    static void encode(uint32_t const* in, uint32_t /*universe*/, uint32_t n,
                       std::vector<uint8_t>& out) {
        uint32_t* src = const_cast<uint32_t*>(in);
        std::vector<uint8_t> buf(streamvbyte_max_compressedbytes(n));
        size_t out_len = streamvbyte_encode(src, n, buf.data());
        out.insert(out.end(), buf.data(), buf.data() + out_len);
    }

    static uint8_t const* decode(uint8_t const* in, uint32_t* out,
                                 uint32_t /*universe*/, size_t n) {
        auto read = streamvbyte_decode(in, out, n);
        return in + read;
    }
};

struct maskedvbyte_block {
    static const uint64_t block_size = constants::block_size;
    static const uint64_t overflow = 512;

    static void encode(uint32_t const* in, uint32_t /*universe*/, uint32_t n,
                       std::vector<uint8_t>& out) {
        uint32_t* src = const_cast<uint32_t*>(in);
        std::vector<uint8_t> buf(2 * n * sizeof(uint32_t));
        size_t out_len = vbyte_encode(src, n, buf.data());
        out.insert(out.end(), buf.data(), buf.data() + out_len);
    }

    static uint8_t const* decode(uint8_t const* in, uint32_t* out,
                                 uint32_t /*universe*/, size_t n) {
        auto read = masked_vbyte_decode(in, out, n);
        return in + read;
    }
};

struct varintgb_block {
    static const uint64_t block_size = constants::block_size;
    static const uint64_t overflow = 512;

    static void encode(uint32_t const* in, uint32_t /*universe*/, uint32_t n,
                       std::vector<uint8_t>& out) {
        thread_local VarIntGB<false> varintgb_codec;
        thread_local std::vector<uint8_t> buf(2 * n * sizeof(uint32_t));
        size_t out_len = varintgb_codec.encodeArray(in, n, buf.data());
        out.insert(out.end(), buf.data(), buf.data() + out_len);
    }

    static uint8_t const* decode(uint8_t const* in, uint32_t* out,
                                 uint32_t /*universe*/, size_t n) {
        thread_local VarIntGB<false> varintgb_codec;
        auto read = varintgb_codec.decodeArray(in, n, out);
        return read + in;
    }
};

struct gamma_block {
    static const uint64_t block_size = constants::block_size;
    static const uint64_t overflow = 0;

    static void encode(uint32_t const* in, uint32_t /* sum_of_values */,
                       size_t n, std::vector<uint8_t>& out) {
        assert(n <= block_size);
        succinct::bit_vector_builder bvb;
        for (size_t i = 0; i != n; ++i, ++in) write_gamma(bvb, *in);
        auto const& bits = bvb.move_bits();
        uint8_t const* bufptr = (uint8_t const*)bits.data();
        uint64_t num_bytes = succinct::util::ceil_div(bvb.size(), 8);
        out.insert(out.end(), bufptr, bufptr + num_bytes);
    }

    static uint8_t const* decode(uint8_t const* in, uint32_t* out,
                                 uint32_t /* sum_of_values */, size_t n) {
        bits_enumerator it(reinterpret_cast<uint64_t const*>(in));
        for (size_t i = 0; i != n; ++i, ++out) *out = read_gamma(it);
        return in + succinct::util::ceil_div(it.position(), 8);
    }
};

struct lex_gamma_block {
    static const uint64_t block_size = constants::block_size;
    static const uint64_t overflow = 0;

    static void encode(uint32_t const* in, uint32_t /* sum_of_values */,
                       size_t n, std::vector<uint8_t>& out) {
        assert(n <= block_size);
        succinct::bit_vector_builder bvb;
        for (size_t i = 0; i != n; ++i, ++in) write_lex_gamma(bvb, *in);
        auto const& bits = bvb.move_bits();
        uint8_t const* bufptr = (uint8_t const*)bits.data();
        uint64_t num_bytes = succinct::util::ceil_div(bvb.size(), 8);
        out.insert(out.end(), bufptr, bufptr + num_bytes);
    }

    static uint8_t const* decode(uint8_t const* in, uint32_t* out,
                                 uint32_t /* sum_of_values */, size_t n) {
        bits_enumerator it(reinterpret_cast<uint64_t const*>(in));
        for (size_t i = 0; i != n; ++i, ++out) *out = read_lex_gamma(it);
        return in + succinct::util::ceil_div(it.position(), 8);
    }
};

struct delta_block {
    static const uint64_t block_size = constants::block_size;
    static const uint64_t overflow = 0;

    static void encode(uint32_t const* in, uint32_t /* sum_of_values */,
                       size_t n, std::vector<uint8_t>& out) {
        assert(n <= block_size);
        succinct::bit_vector_builder bvb;
        for (size_t i = 0; i != n; ++i, ++in) write_delta(bvb, *in);
        auto const& bits = bvb.move_bits();
        uint8_t const* bufptr = (uint8_t const*)bits.data();
        uint64_t num_bytes = succinct::util::ceil_div(bvb.size(), 8);
        out.insert(out.end(), bufptr, bufptr + num_bytes);
    }

    static uint8_t const* decode(uint8_t const* in, uint32_t* out,
                                 uint32_t /* sum_of_values */, size_t n) {
        bits_enumerator it(reinterpret_cast<uint64_t const*>(in));
        for (size_t i = 0; i != n; ++i, ++out) *out = read_delta(it);
        return in + succinct::util::ceil_div(it.position(), 8);
    }
};

struct lex_delta_block {
    static const uint64_t block_size = constants::block_size;
    static const uint64_t overflow = 0;

    static void encode(uint32_t const* in, uint32_t /* sum_of_values */,
                       size_t n, std::vector<uint8_t>& out) {
        assert(n <= block_size);
        succinct::bit_vector_builder bvb;
        for (size_t i = 0; i != n; ++i, ++in) write_lex_delta(bvb, *in);
        auto const& bits = bvb.move_bits();
        uint8_t const* bufptr = (uint8_t const*)bits.data();
        uint64_t num_bytes = succinct::util::ceil_div(bvb.size(), 8);
        out.insert(out.end(), bufptr, bufptr + num_bytes);
    }

    static uint8_t const* decode(uint8_t const* in, uint32_t* out,
                                 uint32_t /* sum_of_values */, size_t n) {
        bits_enumerator it(reinterpret_cast<uint64_t const*>(in));
        for (size_t i = 0; i != n; ++i, ++out) *out = read_lex_delta(it);
        return in + succinct::util::ceil_div(it.position(), 8);
    }
};

struct rice_block {
    static const uint64_t block_size = constants::block_size;
    static const uint64_t overflow = 0;
    static const uint64_t k = 1;
    static const uint64_t divisor = uint64_t(1) << k;

    static void encode(uint32_t const* in, uint32_t /* sum_of_values */,
                       size_t n, std::vector<uint8_t>& out) {
        assert(n <= block_size);
        succinct::bit_vector_builder bvb;
        for (size_t i = 0; i != n; ++i, ++in) {
            write_rice(bvb, *in, k, divisor);
        }
        auto const& bits = bvb.move_bits();
        uint8_t const* bufptr = (uint8_t const*)bits.data();
        uint64_t num_bytes = succinct::util::ceil_div(bvb.size(), 8);
        out.insert(out.end(), bufptr, bufptr + num_bytes);
    }

    static uint8_t const* decode(uint8_t const* in, uint32_t* out,
                                 uint32_t /* sum_of_values */, size_t n) {
        bits_enumerator it(reinterpret_cast<uint64_t const*>(in));
        for (size_t i = 0; i != n; ++i, ++out) {
            *out = read_rice(it, k, divisor);
        }
        return in + succinct::util::ceil_div(it.position(), 8);
    }
};

struct zeta_block {
    static const uint64_t block_size = constants::block_size;
    static const uint64_t overflow = 0;
    static const uint64_t k = 2;  // k = 1 equals Elias' gamma

    static void encode(uint32_t const* in, uint32_t /* sum_of_values */,
                       size_t n, std::vector<uint8_t>& out) {
        assert(n <= block_size);
        succinct::bit_vector_builder bvb;
        for (size_t i = 0; i != n; ++i, ++in) {
            write_zeta(bvb, *in, k);
        }
        auto const& bits = bvb.move_bits();
        uint8_t const* bufptr = (uint8_t const*)bits.data();
        uint64_t num_bytes = succinct::util::ceil_div(bvb.size(), 8);
        out.insert(out.end(), bufptr, bufptr + num_bytes);
    }

    static uint8_t const* decode(uint8_t const* in, uint32_t* out,
                                 uint32_t /* sum_of_values */, size_t n) {
        bits_enumerator it(reinterpret_cast<uint64_t const*>(in));
        for (size_t i = 0; i != n; ++i, ++out) {
            *out = read_zeta(it, k);
        }
        return in + succinct::util::ceil_div(it.position(), 8);
    }
};
}  // namespace ds2i
