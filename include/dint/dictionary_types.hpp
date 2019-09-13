#pragma once

#include "dint_configuration.hpp"
#include "rectangular_dictionary.hpp"
#include "single_dictionary.hpp"
#include "multi_dictionary.hpp"

namespace ds2i {

using single_dictionary_rectangular_type =
    rectangular_dictionary<constants::num_entries, constants::max_entry_size>;
using single_dictionary_packed_type =
    single_dictionary<constants::num_entries, constants::max_entry_size,
                      pack_policy>;
using single_dictionary_overlapped_type =
    single_dictionary<constants::num_entries, constants::max_entry_size,
                      overlap_policy>;

using multi_dictionary_packed_type =
    multi_dictionary<constants::num_entries, constants::max_entry_size,
                     pack_policy>;
using multi_dictionary_overlapped_type =
    multi_dictionary<constants::num_entries, constants::max_entry_size,
                     overlap_policy>;

}  // namespace ds2i
