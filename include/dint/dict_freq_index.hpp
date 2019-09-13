#pragma once

#include <succinct/mappable_vector.hpp>
#include <succinct/bit_vector.hpp>

#include "util.hpp"

#include "dictionary_builders.hpp"
#include "compact_elias_fano.hpp"
#include "dict_posting_list.hpp"
#include "block_statistics.hpp"

namespace ds2i {

template <typename DictionaryBuilder, typename Coder>
struct dict_freq_index {
    using dictionary_builder = DictionaryBuilder;
    using dictionary_type = typename dictionary_builder::dictionary_type;
    using coder_type = Coder;

    typedef dict_posting_list<dictionary_type, coder_type> sequence_type;

    typedef typename sequence_type::document_enumerator document_enumerator;

    struct builder {
        builder(uint64_t num_docs, global_parameters const& params)
            : m_num_docs(num_docs)
            , m_params(params)
            , m_queue(1 << 24) {
            m_endpoints.push_back(0);
        }

        template <typename DocsIterator, typename FreqsIterator>
        void add_posting_list(uint64_t n, DocsIterator docs_begin,
                              FreqsIterator freqs_begin,
                              uint64_t /* occurrences */) {
            if (!n) throw std::invalid_argument("List must be nonempty");
            std::shared_ptr<list_adder<DocsIterator, FreqsIterator>> ptr(
                new list_adder<DocsIterator, FreqsIterator>(*this, docs_begin,
                                                            freqs_begin, n));
            m_queue.add_job(ptr, 2 * n);
        }

        void build_model(std::string const& prefix_name) {
            logger() << "building or loading dictionary for docs..."
                     << std::endl;
            build_or_load_dict(m_docs_dict_builder, prefix_name,
                               data_type::docs);
            logger() << "DONE" << std::endl;

            logger() << "building or loading dictionary for freqs..."
                     << std::endl;
            build_or_load_dict(m_freqs_dict_builder, prefix_name,
                               data_type::freqs);
            logger() << "DONE" << std::endl;

            m_docs_dict_builder.prepare_for_encoding();
            m_freqs_dict_builder.prepare_for_encoding();
        }

        void build(dict_freq_index& dfi) {
            m_queue.complete();
            dfi.m_params = m_params;
            dfi.m_size = m_endpoints.size() - 1;
            dfi.m_num_docs = m_num_docs;
            dfi.m_lists.steal(m_lists);
            m_docs_dict_builder.build(dfi.m_docs_dict);
            m_freqs_dict_builder.build(dfi.m_freqs_dict);
            succinct::bit_vector_builder bvb;
            compact_elias_fano::write(bvb, m_endpoints.begin(),
                                      dfi.m_lists.size(), dfi.m_size, m_params);
            succinct::bit_vector(&bvb).swap(dfi.m_endpoints);
        }

    private:
        template <typename DocsIterator, typename FreqsIterator>
        struct list_adder : semiasync_queue::job {
            list_adder(builder& b, DocsIterator docs_begin,
                       FreqsIterator freqs_begin, uint64_t n)
                : b(b)
                , docs_begin(docs_begin)
                , freqs_begin(freqs_begin)
                , n(n) {}

            virtual void prepare() {
                sequence_type::write(b.m_docs_dict_builder,
                                     b.m_freqs_dict_builder, lists, n,
                                     docs_begin, freqs_begin);
            }

            virtual void commit() {
                b.m_lists.insert(b.m_lists.end(), lists.begin(), lists.end());
                b.m_endpoints.push_back(b.m_lists.size());
            }

            builder& b;
            std::vector<uint8_t> lists;
            DocsIterator docs_begin;
            FreqsIterator freqs_begin;
            uint64_t n;
        };

        uint64_t m_num_docs;
        global_parameters m_params;
        semiasync_queue m_queue;
        std::vector<uint64_t> m_endpoints;
        std::vector<uint8_t> m_lists;
        typename dictionary_type::builder m_docs_dict_builder;
        typename dictionary_type::builder m_freqs_dict_builder;

        void build_or_load_dict(typename dictionary_type::builder& builder,
                                std::string prefix_name, data_type dt) {
            std::string file_name = prefix_name + extension(dt);
            using namespace boost::filesystem;
            path p(file_name);
            using d_type = typename dictionary_type::builder;
            std::string dictionary_file = "./dict." + p.filename().string() +
                                          "." + d_type::type() + "." +
                                          dictionary_builder::type();

            if (boost::filesystem::exists(dictionary_file)) {
                builder.load_from_file(dictionary_file);
            } else {
                using statistics_type =
                    typename dictionary_builder::statistics_type;
                auto statistics = statistics_type::create_or_load(
                    prefix_name, dt, dictionary_builder::filter());
                dictionary_builder::build(builder, statistics);
                if (!builder.try_store_to_file(dictionary_file)) {
                    logger() << "cannot write dictionary to file";
                }
            }
        }
    };

    dict_freq_index()
        : m_size(0)
        , m_num_docs(0) {}

    size_t size() const {  // num. of lists
        return m_size;
    }

    uint64_t num_docs() const {  // universe
        return m_num_docs;
    }

    document_enumerator operator[](size_t i) const {
        assert(i < size());
        compact_elias_fano::enumerator endpoints(m_endpoints, 0, m_lists.size(),
                                                 m_size, m_params);
        auto endpoint = endpoints.move(i).second;
        return document_enumerator(&m_docs_dict, &m_freqs_dict,
                                   m_lists.data() + endpoint, num_docs(), i);
    }

    uint32_t decode(size_t i, uint32_t* out) const {
        assert(i < size());
        compact_elias_fano::enumerator endpoints(m_endpoints, 0, m_lists.size(),
                                                 m_size, m_params);
        auto endpoint = endpoints.move(i).second;
        return sequence_type::decode(&m_docs_dict, &m_freqs_dict,
                                     m_lists.data() + endpoint, out);
    }

    void warmup(size_t i) {
        assert(i < size());
        compact_elias_fano::enumerator endpoints(m_endpoints, 0, m_lists.size(),
                                                 m_size, m_params);
        auto begin = endpoints.move(i).second;
        auto end = m_lists.size();
        if (i + 1 != size()) {
            end = endpoints.move(i + 1).second;
        }

        volatile uint32_t tmp;
        for (size_t i = begin; i != end; ++i) {
            tmp = m_lists[i];
        }
        (void)tmp;
    }

    void swap(dict_freq_index& other) {
        std::swap(m_params, other.m_params);
        std::swap(m_size, other.m_size);
        m_endpoints.swap(other.m_endpoints);
        m_lists.swap(other.m_lists);
        m_docs_dict.swap(other.m_docs_dict);
        m_freqs_dict.swap(other.m_freqs_dict);
    }

    template <typename Visitor>
    void map(Visitor& visit) {
        visit(m_params, "m_params")(m_size, "m_size")(m_num_docs, "m_num_docs")(
            m_endpoints, "m_endpoints")(m_lists, "m_lists")(
            m_docs_dict, "m_docs_dict")(m_freqs_dict, "m_freqs_dict");
    }

private:
    global_parameters m_params;
    size_t m_size;
    size_t m_num_docs;
    succinct::bit_vector m_endpoints;
    succinct::mapper::mappable_vector<uint8_t> m_lists;
    dictionary_type m_docs_dict;
    dictionary_type m_freqs_dict;
};
}  // namespace ds2i
