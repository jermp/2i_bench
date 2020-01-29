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

typedef freq_index<partitioned_sequence<maskedvbyte_block>,
                   // positive_sequence<partitioned_sequence<maskedvbyte_block>>
                   ds2i::positive_sequence<>>
    opt_vb;

typedef freq_index<partitioned_sequence<delta_block>,
                   // positive_sequence<partitioned_sequence<delta_block>>
                   ds2i::positive_sequence<>>
    opt_delta;

}  // namespace pvb