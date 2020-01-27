#pragma once

#include <boost/preprocessor/seq/for_each.hpp>
#include <boost/preprocessor/stringize.hpp>
#include <boost/preprocessor/cat.hpp>

#include "freq_index.hpp"
#include "positive_sequence.hpp"
#include "partitioned_sequence.hpp"
#include "uniform_partitioned_sequence.hpp"
#include "binary_freq_collection.hpp"
#include "block_freq_index.hpp"
#include "block_codecs.hpp"
#include "mixed_block.hpp"

#include "dint_configuration.hpp"
#include "dint_codecs.hpp"
#include "dictionary_types.hpp"
#include "dictionary_builders.hpp"
#include "block_statistics.hpp"
#include "dict_freq_index.hpp"

#include "opt_vbyte/types.hpp"

namespace ds2i {

// elias-fano-based indexes
typedef freq_index<indexed_sequence, positive_sequence<>> ef_index;
typedef freq_index<
    uniform_partitioned_sequence<>,
    positive_sequence<uniform_partitioned_sequence<strict_sequence>>>
    pef_uniform_index;
typedef freq_index<partitioned_sequence<>,
                   positive_sequence<partitioned_sequence<strict_sequence>>>
    pef_opt_index;

// pfor-based indexes
typedef block_freq_index<optpfor_block> optpfor_index;

// binary interpolative coding index
typedef block_freq_index<interpolative_block> bic_index;

// qmx index
typedef block_freq_index<qmx_block> qmx_index;

// simple-based indexes
typedef block_freq_index<simple9_block> simple9_index;
typedef block_freq_index<simple16_block> simple16_index;
typedef block_freq_index<simple8b_block> simple8b_index;

// vbyte-based indexes
typedef block_freq_index<vbyte_block> vbyte_index;
typedef block_freq_index<varint_G8IU_block> varintg8iu_index;
typedef block_freq_index<varintgb_block> varintgb_index;
typedef block_freq_index<maskedvbyte_block> maskedvbyte_index;
typedef block_freq_index<streamvbyte_block> streamvbyte_index;
typedef pvb::opt_vb opt_vbyte_index;

// universal-code-based indexes
typedef block_freq_index<gamma_block> gamma_index;
typedef block_freq_index<delta_block> delta_index;
typedef block_freq_index<delta_table_block> delta_table_index;
typedef block_freq_index<rice_block> rice_index;
typedef block_freq_index<zeta_block> zeta_index;

// DINT-based indexes
using adjusted_collector_type = adjusted<constants::max_entry_size>;
using adjusted_block_stats_type = block_statistics<adjusted_collector_type>;
using adjusted_block_multi_stats_type =
    block_multi_statistics<adjusted_collector_type>;
using single_rectangular_builder =
    decreasing_static_frequencies<single_dictionary_rectangular_type,
                                  adjusted_block_stats_type>;
using single_packed_builder =
    decreasing_static_frequencies<single_dictionary_packed_type,
                                  adjusted_block_stats_type>;
using multi_packed_builder =
    decreasing_static_frequencies<multi_dictionary_packed_type,
                                  adjusted_block_multi_stats_type>;

// DINT configurations (all use optimal block parsing)
using single_rect_dint_index =
    dict_freq_index<single_rectangular_builder, opt_dint_single_dict_block>;
using single_packed_dint_index =
    dict_freq_index<single_packed_builder, opt_dint_single_dict_block>;
using multi_packed_dint_index =
    dict_freq_index<multi_packed_builder, opt_dint_multi_dict_block>;
}  // namespace ds2i

#define DS2I_INDEX_TYPES                                                  \
    (ef)(pef_uniform)(pef_opt)(optpfor)(bic)(qmx)(simple9)(simple16)(     \
        simple8b)(vbyte)(varintg8iu)(varintgb)(maskedvbyte)(streamvbyte)( \
        gamma)(delta)(delta_table)(rice)(zeta)(single_rect_dint)(         \
        single_packed_dint)(multi_packed_dint)(opt_vbyte)
