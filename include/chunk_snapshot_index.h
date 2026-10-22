#pragma once

#include <string>
#include <unordered_map>
#include <vector>

namespace ecleaner {
// Borrows a single immutable reconciliation snapshot; never survives applyTriggers.
// Build O(N), query O(1 + matching records) on average, with at most nine
// chunk lookups for a 3x3 trigger. No actor is deferred to a later tick.
template <typename Key, typename Hash, typename Record>
class ChunkSnapshotIndex {
    struct Bucket {
        std::vector<const Record *> all;
        std::vector<const Record *> cleanable;
        std::unordered_map<std::string, std::vector<const Record *>> types;
    };
    std::unordered_map<Key, Bucket, Hash> chunks_;
public:
    explicit ChunkSnapshotIndex(const std::vector<Record> &records)
    {
        for (const auto &record : records) {
            auto &bucket = chunks_[record.chunk];
            bucket.all.push_back(&record);
            if (record.cleanable) bucket.cleanable.push_back(&record);
            bucket.types[record.type].push_back(&record);
        }
    }

    template <typename Visitor>
    void visit(const Key &key, bool cleanable_only, const std::string *type, Visitor visitor) const
    {
        const auto it = chunks_.find(key);
        if (it == chunks_.end()) return;
        const auto &bucket = it->second;
        if (type) {
            const auto typed = bucket.types.find(*type);
            if (typed != bucket.types.end()) {
                for (const auto *record : typed->second) visitor(*record);
            }
        }
        else {
            for (const auto *record : cleanable_only ? bucket.cleanable : bucket.all) visitor(*record);
        }
    }
};
}  // namespace ecleaner
