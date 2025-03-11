#ifndef CONCURRENT_HASH_MAP_HPP
#define CONCURRENT_HASH_MAP_HPP

#include <iostream>
#include <vector>
#include <mutex>
#include <shared_mutex>
#include <functional>
#include <list>
#include <optional>

template <typename Key, typename Value, typename Hash = std::hash<Key>>
class ConcurrentHashMap {
private:
    struct Node {
        Key key;
        Value value;
    };

    // Each bucket has associated mutex (given by same index in respective vectors)
    // Locking granularity is of each bucket
    std::vector<std::list<Node>> buckets;
    std::vector<std::shared_mutex> mutexes;
    Hash hash_function;

    // Get mutex for the bucket mapped by the given key
    std::shared_mutex& get_mutex(const Key& key) {
        std::size_t index = hash_function(key) % buckets.size();

        return mutexes[index];
    }

    std::list<Node>& get_bucket(const Key& key) {
        std::size_t index = hash_function(key) % buckets.size();

        return buckets[index];
    }

public:
    explicit ConcurrentHashMap(std::size_t num_buckets = 16) : buckets(num_buckets), mutexes(num_buckets) {}

    // Insert key-value pair into map
    Value insert(const Key& key, const Value& value) {
        std::unique_lock lock(get_mutex(key)); // Only single writer at a time for each bucket

        auto& bucket = get_bucket(key);
        auto it = std::find_if(bucket.begin(), bucket.end(), [&](const Node& node) { return node.key == key; });

        if (it != bucket.end()) {
            return it->value;
        } else {
            // Key doesn't exist, insert new key-value pair
            bucket.push_back({key, value});
            return value;
        }
    }

    std::optional<Value> get(const Key& key) {
        std::shared_lock lock(get_mutex(key));

        const auto& bucket = get_bucket(key);
        auto it = std::find_if(bucket.begin(), bucket.end(), [&](const Node& node) {
            return node.key == key;
        });
        if (it != bucket.end()) {
            return it->value;
        }
        return std::nullopt;
    }

    // Remove a key-value pair from the hash map
    void remove(const Key& key) {
        std::unique_lock lock(get_mutex(key));

        auto& bucket = get_bucket(key);
        bucket.remove_if([&](const Node& node) { return node.key == key; });
    }

    void print() {
        for (std::size_t i = 0; i < buckets.size(); ++i) {
            std::shared_lock lock(mutexes[i]);

            std::cout << "Bucket " << i << ": ";
            const auto& bucket = buckets[i];
            for (const auto& node : bucket) {
                std::cout << "(" << node.key << ", " << node.value << ") ";
            }
            std::cout << std::endl;
        }
    }

    ~ConcurrentHashMap() = default;
};

#endif 
