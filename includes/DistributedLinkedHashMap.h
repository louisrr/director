#ifndef DISTRIBUTEDLINKEDHASHMAP_H
#define DISTRIBUTEDLINKEDHASHMAP_H

#include <seastar/core/distributed.hh>
#include <seastar/core/future.hh>
#include <unordered_map>

template<typename K, typename V>
class DistributedLinkedHashMap {
public:
    seastar::future<> start();
    seastar::future<> stop();
    seastar::future<> put(const K& key, const V& value);
    seastar::future<V> get(const K& key);
    seastar::future<> remove(const K& key);
    
private:
    size_t get_shard_id(const K& key);
    seastar::future<> replicate_put(const K& key, const V& value);
    seastar::future<> replicate_remove(const K& key);
    seastar::future<> save_all_shards();
    seastar::future<> load_all_shards();

    seastar::distributed<std::unordered_map<K, V>> maps;
};

#endif // DISTRIBUTEDLINKEDHASHMAP_H
