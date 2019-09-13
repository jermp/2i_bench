#define BOOST_TEST_MODULE block_codecs

#include "succinct/test_common.hpp"
#include "block_codecs.hpp"
#include <vector>
#include <cstdlib>
#include <random>

template <typename BlockCodec>
void test_block_codec() {
    std::vector<size_t> sizes = {1, 16, BlockCodec::block_size - 1,
                                 BlockCodec::block_size};
    for (size_t mag = 1; mag < 25; mag++) {
        for (auto size : sizes) {
            std::vector<uint32_t> values(size);
            std::mt19937 gen(12345);
            std::uniform_int_distribution<uint32_t> dis(0, (1 << mag));
            std::generate(values.begin(), values.end(),
                          [&]() { return dis(gen); });
            for (size_t tcase = 0; tcase < 2; ++tcase) {
                // test both undefined and given sum_of_values
                uint32_t sum_of_values(-1);
                if (tcase == 1) {
                    sum_of_values =
                        std::accumulate(values.begin(), values.end(), 0);
                }
                std::vector<uint8_t> encoded;
                BlockCodec::encode(values.data(), sum_of_values, values.size(),
                                   encoded);

                std::vector<uint32_t> decoded(values.size() +
                                              BlockCodec::overflow);
                uint8_t const* out =
                    BlockCodec::decode(encoded.data(), decoded.data(),
                                       sum_of_values, values.size());

                BOOST_REQUIRE_EQUAL(encoded.size(), out - encoded.data());
                BOOST_REQUIRE_EQUAL_COLLECTIONS(
                    values.begin(), values.end(), decoded.begin(),
                    decoded.begin() + values.size());
            }
        }
    }
}

BOOST_AUTO_TEST_CASE(block_codecs) {
    test_block_codec<ds2i::optpfor_block>();
    test_block_codec<ds2i::varint_G8IU_block>();
    test_block_codec<ds2i::interpolative_block>();
    test_block_codec<ds2i::qmx_block>();
    test_block_codec<ds2i::vbyte_block>();
    test_block_codec<ds2i::simple16_block>();
}
