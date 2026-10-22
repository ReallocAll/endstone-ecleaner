#include "chunk_snapshot_index.h"
#include <cstdlib>
#include <set>
#include <tuple>

struct Key {
    int dimension, x, z;
    bool operator==(const Key &) const = default;
};
struct Hash {
    std::size_t operator()(const Key &key) const { return std::hash<int>{}(key.x * 97 + key.z * 13 + key.dimension); }
};
struct Record { Key chunk; std::string type; bool cleanable; int id; };

int main()
{
    std::vector<Record> records;
    for (int d = 0; d < 2; ++d)
        for (int x = -30; x <= 30; ++x)
            for (int z = -30; z <= 30; ++z)
                for (int n = 0; n < 3; ++n)
                    records.push_back({{d,x,z}, n == 0 ? "slime" : "wolf", n == 0, static_cast<int>(records.size())});
    const ecleaner::ChunkSnapshotIndex<Key, Hash, Record> index(records);
    std::size_t visits = 0;
    for (int mode = 0; mode < 4; ++mode) {
        for (int x = -10; x <= 10; ++x) {
            const Key center{0,x,-3};
            const std::string type = "wolf";
            std::set<int> actual, expected;
            const auto visitor = [&](const Record &r) { ++visits; actual.insert(r.id); };
            if (mode == 3) {
                for (int dx = -1; dx <= 1; ++dx)
                    for (int dz = -1; dz <= 1; ++dz)
                        index.visit({center.dimension,center.x+dx,center.z+dz}, true, nullptr, visitor);
            }
            else index.visit(center, mode == 1, mode == 0 ? &type : nullptr, visitor);
            for (const auto &r : records) {
                const bool match = mode == 3
                    ? r.cleanable && r.chunk.dimension == center.dimension
                      && std::abs(r.chunk.x-center.x) <= 1 && std::abs(r.chunk.z-center.z) <= 1
                    : r.chunk == center && (mode != 0 || r.type == type) && (mode != 1 || r.cleanable);
                if (match) expected.insert(r.id);
            }
            if (actual != expected) std::abort();
        }
    }
    // 84 triggers over 22,326 actors: exactly 315 matching records, no unrelated visits.
    if (visits != 315) std::abort();
}
