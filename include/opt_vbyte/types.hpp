#pragma once

#include <boost/preprocessor/cat.hpp>
#include <boost/preprocessor/seq/for_each.hpp>
#include <boost/preprocessor/stringize.hpp>

#include "binary_freq_collection.hpp"
#include "block_codecs.hpp"
#include "freq_index.hpp"
#include "partitioned_vb_sequence.hpp"
#include "positive_sequence.hpp"
#include "indexed_sequence.hpp"

namespace pvb {
typedef freq_index<
    partitioned_vb_sequence<maskedvbyte_block>,
    positive_sequence<partitioned_vb_sequence<maskedvbyte_block>>>
    opt_vb;
}