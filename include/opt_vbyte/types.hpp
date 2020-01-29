#pragma once

#include <boost/preprocessor/cat.hpp>
#include <boost/preprocessor/seq/for_each.hpp>
#include <boost/preprocessor/stringize.hpp>

#include "binary_freq_collection.hpp"
#include "block_codecs.hpp"
#include "freq_index.hpp"
#include "partitioned_sequence.hpp"
#include "positive_sequence.hpp"
#include "indexed_sequence.hpp"

namespace pvb {

#define OPT_INDEX(CODEC)                            \
    freq_index<partitioned_sequence<CODEC##_block>, \
               positive_sequence<partitioned_sequence<CODEC##_block>>>

typedef OPT_INDEX(maskedvbyte) opt_vb;
typedef OPT_INDEX(delta) opt_delta;

}  // namespace pvb