#pragma once

#include <succinct/mappable_vector.hpp>
#include <succinct/bit_vector.hpp>
#include "compact_elias_fano.hpp"
#include "block_posting_list.hpp"
#include "semiasync_queue.hpp"

namespace ds2i {

template <typename BlockCodec, bool Profile = false>
class block_freq_index {
public:
    block_freq_index()
        : m_size(0) {}

    class builder {
    public:
        builder(uint64_t num_docs, global_parameters const& params)
            : m_queue(1 << 24)
            , m_params(params) {
            m_num_docs = num_docs;
            m_endpoints.push_back(0);
        }

        template <typename DocsIterator, typename FreqsIterator>
        void add_posting_list(uint64_t n, DocsIterator docs_begin,
                              FreqsIterator freqs_begin,
                              uint64_t /* occurrences */) {
            if (!n)
                throw std::invalid_argument("List must be nonempty");
            block_posting_list<BlockCodec, Profile>::write(
                m_lists, n, docs_begin, freqs_begin);
            m_endpoints.push_back(m_lists.size());

            // if (!n) throw std::invalid_argument("List must be nonempty");
            // std::shared_ptr<list_adder<DocsIterator, FreqsIterator>>
            //     ptr(new list_adder<DocsIterator, FreqsIterator>
            //         (*this, docs_begin, freqs_begin, n));
            // m_queue.add_job(ptr, 2 * n);
        }

        template <typename BlockDataRange>
        void add_posting_list(uint64_t n, BlockDataRange const& blocks) {
            if (!n)
                throw std::invalid_argument("List must be nonempty");
            block_posting_list<BlockCodec>::write_blocks(m_lists, n, blocks);
            m_endpoints.push_back(m_lists.size());
        }

        template <typename BytesRange>
        void add_posting_list(BytesRange const& data) {
            m_lists.insert(m_lists.end(), std::begin(data), std::end(data));
            m_endpoints.push_back(m_lists.size());
        }

        void build_model(std::string const&) {}

        void build(block_freq_index& sq) {
            m_queue.complete();
            sq.m_params = m_params;
            sq.m_size = m_endpoints.size() - 1;
            sq.m_num_docs = m_num_docs;
            sq.m_lists.steal(m_lists);

            succinct::bit_vector_builder bvb;
            compact_elias_fano::write(bvb, m_endpoints.begin(),
                                      sq.m_lists.size(), sq.m_size,
                                      m_params);  // XXX
            succinct::bit_vector(&bvb).swap(sq.m_endpoints);
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
                block_posting_list<BlockCodec, Profile>::write(
                    list, n, docs_begin, freqs_begin);
            }

            virtual void commit() {
                b.m_lists.insert(b.m_lists.end(), list.begin(), list.end());
                b.m_endpoints.push_back(b.m_lists.size());
            }

            builder& b;
            DocsIterator docs_begin;
            FreqsIterator freqs_begin;
            uint64_t n;
            std::vector<uint8_t> list;
        };

        semiasync_queue m_queue;
        global_parameters m_params;
        size_t m_num_docs;
        std::vector<uint64_t> m_endpoints;
        std::vector<uint8_t> m_lists;
    };

    size_t size() const {
        return m_size;
    }

    uint64_t num_docs() const {
        return m_num_docs;
    }

    typedef
        typename block_posting_list<BlockCodec, Profile>::document_enumerator
            document_enumerator;

    document_enumerator operator[](size_t i) const {
        assert(i < size());
        compact_elias_fano::enumerator endpoints(m_endpoints, 0, m_lists.size(),
                                                 m_size, m_params);
        auto endpoint = endpoints.move(i).second;
        return document_enumerator(m_lists.data() + endpoint, num_docs(), i);
    }

    uint32_t decode(size_t i, uint32_t* out) const {
        assert(i < size());
        compact_elias_fano::enumerator endpoints(m_endpoints, 0, m_lists.size(),
                                                 m_size, m_params);
        auto endpoint = endpoints.move(i).second;
        return block_posting_list<BlockCodec, Profile>::decode(
            m_lists.data() + endpoint, out);
    }

    void warmup(size_t i) const {
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

    void swap(block_freq_index& other) {
        std::swap(m_params, other.m_params);
        std::swap(m_size, other.m_size);
        m_endpoints.swap(other.m_endpoints);
        m_lists.swap(other.m_lists);
    }

    template <typename Visitor>
    void map(Visitor& visit) {
        visit(m_params, "m_params")(m_size, "m_size")(m_num_docs, "m_num_docs")(
            m_endpoints, "m_endpoints")(m_lists, "m_lists");
    }

private:
    global_parameters m_params;
    size_t m_size;
    size_t m_num_docs;
    succinct::bit_vector m_endpoints;
    succinct::mapper::mappable_vector<uint8_t> m_lists;
};
}  // namespace ds2i
