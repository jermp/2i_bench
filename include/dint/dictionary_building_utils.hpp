#pragma once

#include <queue>
#include <algorithm>

#include <boost/progress.hpp>

namespace ds2i {

template <class t_entry>
bool prefix_overlap(t_entry& src, t_entry& target) {
    return std::equal(src.begin(), src.end(), target.begin(),
                      target.begin() + src.size());
}

template <class t_entry>
bool is_contained(t_entry& src, t_entry& target) {
    return std::search(target.begin(), target.end(), src.begin(), src.end()) !=
           target.end();
}

struct overlap_t {
    size_t left;
    size_t right;
    size_t overlap;
    overlap_t(size_t l, size_t r, size_t o) : left(l), right(r), overlap(o) {}
    bool operator<(const overlap_t& other) const {
        return overlap < other.overlap;
    }
};

struct target_t {
    template <class t_itr>
    target_t(t_itr begin, t_itr end) : entry(begin, end){};

    bool operator<(const target_t& other) const {
        if (entry.size() == other.entry.size()) {
            return std::lexicographical_compare(entry.begin(), entry.end(),
                                                other.entry.begin(),
                                                other.entry.end());
        }
        return entry.size() < other.entry.size();
    }

    bool operator==(const target_t& other) const {
        if (entry.size() != other.entry.size())
            return false;
        return std::equal(entry.begin(), entry.end(), other.entry.begin(),
                          other.entry.end());
    }

    bool operator!=(const target_t& other) const {
        if (entry.size() != other.entry.size())
            return true;
        return std::equal(entry.begin(), entry.end(), other.entry.begin(),
                          other.entry.end());
    }

    std::vector<uint32_t> entry;
    bool valid = true;
};

size_t compute_overlap(const std::vector<uint32_t>& A,
                       const std::vector<uint32_t>& B) {
    auto min_len = std::min(A.size(), B.size());
    for (size_t l = 1; l <= min_len; l++) {
        auto start = A.end() - l;
        if (!std::equal(start, A.end(), B.begin())) {
            return l - 1;
        }
    }
    return min_len;
}

std::priority_queue<overlap_t> compute_all_overlaps(
    std::vector<target_t>& entries) {
    std::priority_queue<overlap_t> pq;
    for (size_t i = 0; i < entries.size(); i++) {
        for (size_t j = 0; j < entries.size(); j++) {
            if (i != j) {
                auto overlap =
                    compute_overlap(entries[i].entry, entries[j].entry);
                if (overlap > 1) {
                    pq.emplace(i, j, overlap);
                }
            }
        }
    }
    return pq;
}

void perform_greedy_prefix_suffix_overlap(std::vector<target_t>& entries) {
    std::cout << "compute_all_overlaps" << std::endl;
    auto overlaps = compute_all_overlaps(entries);
    std::cout << "overlaps = " << overlaps.size() << std::endl;
    std::cout << "merge overlapping entries" << std::endl;
    size_t merges = 0;
    size_t overlap_sum = 0;
    while (!overlaps.empty()) {
        auto cur = overlaps.top();
        overlaps.pop();
        if (!entries[cur.left].valid || !entries[cur.right].valid) {
            continue;
        }

        // std::cout << merges << " overlaps = " << overlaps.size() << " " <<
        // cur.overlap << std::endl;

        // (a) merge the nodes
        overlap_sum += cur.overlap;
        auto& node_left = entries[cur.left];
        auto copy_itr = entries[cur.right].entry.begin() + cur.overlap;
        std::copy(copy_itr, entries[cur.right].entry.end(),
                  std::back_inserter(node_left.entry));
        target_t new_node = entries[cur.left];
        entries.push_back(new_node);
        // (b) remove the right node
        entries[cur.left].valid = false;
        entries[cur.right].valid = false;
        for (size_t j = 0; j < entries.size() - 1; j++) {
            if (!entries[j].valid)
                continue;
            auto overlap_left =
                compute_overlap(entries[j].entry, entries.back().entry);
            auto overlap_right =
                compute_overlap(entries.back().entry, entries[j].entry);
            if (overlap_left > 1) {
                overlaps.emplace(j, entries.size() - 1, overlap_left);
            }
            if (overlap_right > 1) {
                overlaps.emplace(entries.size() - 1, j, overlap_right);
            }
        }
        ++merges;
    }

    auto itr = entries.begin();
    while (itr != entries.end()) {
        if (itr->valid == false) {
            itr = entries.erase(itr);
        } else {
            ++itr;
        }
    }

    // perform single overlap merging
    std::cout << "perform single overlap merging" << std::endl;
    size_t total_merges_performed = 0;
    for (size_t i = 0; i < entries.size(); i++) {
        if (entries[i].valid == false)
            continue;
        auto last_sym = entries[i].entry.back();
        for (size_t j = 0; j < entries.size(); j++) {
            if (i != j) {
                if (entries[j].valid == false)
                    continue;
                auto first_sym = entries[j].entry.front();
                if (first_sym == last_sym) {
                    entries[j].valid = false;
                    total_merges_performed++;
                    std::copy(entries[j].entry.begin() + 1,
                              entries[j].entry.end(),
                              std::back_inserter(entries[i].entry));
                    break;
                }
            }
        }
    }
    std::cout << "singleton merges = " << total_merges_performed << std::endl;

    itr = entries.begin();
    while (itr != entries.end()) {
        if (itr->valid == false) {
            itr = entries.erase(itr);
        } else {
            ++itr;
        }
    }

    std::cout << "u32 overlap sum = " << overlap_sum << std::endl;
    std::cout << "non overlapping entries = " << entries.size() << std::endl;
}

struct overlap_policy {
    static std::string type() {
        return "overlapped";
    }

    static std::vector<target_t> compact(
        std::vector<std::vector<target_t>> const& targets) {
        std::vector<target_t> all_targets;
        for (auto& t : targets) {
            std::copy(t.begin(), t.end(), std::back_inserter(all_targets));
        }
        std::cout << "all targets = " << all_targets.size() << std::endl;
        std::sort(all_targets.begin(), all_targets.end());
        std::cout << "removing identical targets " << std::endl;
        auto last = std::unique(all_targets.begin(), all_targets.end());
        all_targets.erase(last, all_targets.end());
        std::cout << "all unique targets = " << all_targets.size() << std::endl;
        {
            std::cout << "find substr overlaps" << std::endl;
            boost::progress_display progress(all_targets.size());
            for (size_t i = 0; i < all_targets.size(); i++) {
                auto& cur = all_targets[i];
                for (size_t j = 0; j < all_targets.size(); j++) {
                    auto& other = all_targets[j];
                    if (i != j && other.valid &&
                        cur.entry.size() < other.entry.size()) {
                        if (is_contained(cur.entry, other.entry)) {
                            cur.valid = false;
                            break;
                        }
                    }
                }
                ++progress;
            }
        }

        std::cout << "remove substr overlaps" << std::endl;
        size_t size_before = all_targets.size();
        auto itr = all_targets.begin();
        while (itr != all_targets.end()) {
            if (itr->valid == false) {
                itr = all_targets.erase(itr);
            } else {
                ++itr;
            }
        }
        size_t size_after = all_targets.size();
        std::cout << "before = " << size_before << std::endl;
        std::cout << "after = " << size_after << std::endl;
        std::cout << "removed = " << size_before - size_after << std::endl;

        std::cout << "find prefix-suffix overlaps" << std::endl;
        perform_greedy_prefix_suffix_overlap(all_targets);
        return all_targets;
    }
};

struct pack_policy {
    static std::string type() {
        return "packed";
    }

    static std::vector<target_t> compact(
        std::vector<std::vector<target_t>> const& targets) {
        std::vector<target_t> all_targets;
        for (auto& t : targets) {
            std::copy(t.begin(), t.end(), std::back_inserter(all_targets));
        }
        std::cout << "all targets = " << all_targets.size() << std::endl;
        std::sort(all_targets.begin(), all_targets.end());
        std::cout << "removing identical targets " << std::endl;
        auto last = std::unique(all_targets.begin(), all_targets.end());
        all_targets.erase(last, all_targets.end());
        std::cout << "all unique targets = " << all_targets.size() << std::endl;
        {
            std::cout << "find prefix overlaps" << std::endl;
            boost::progress_display progress(all_targets.size());
            for (size_t i = 0; i < all_targets.size(); i++) {
                auto& cur = all_targets[i];
                for (size_t j = 0; j < all_targets.size(); j++) {
                    auto& other = all_targets[j];
                    if (i != j && other.valid &&
                        cur.entry.size() < other.entry.size()) {
                        if (prefix_overlap(cur.entry, other.entry)) {
                            cur.valid = false;
                            break;
                        }
                    }
                }
                ++progress;
            }
        }

        std::cout << "remove prefix overlaps" << std::endl;
        size_t size_before = all_targets.size();
        auto itr = all_targets.begin();
        while (itr != all_targets.end()) {
            if (itr->valid == false) {
                itr = all_targets.erase(itr);
            } else {
                ++itr;
            }
        }
        size_t size_after = all_targets.size();
        std::cout << "before = " << size_before << std::endl;
        std::cout << "after = " << size_after << std::endl;
        std::cout << "removed = " << size_before - size_after << std::endl;
        return all_targets;
    }
};
}  // namespace ds2i
