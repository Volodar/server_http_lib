// Simple LRU cache for string key/value with per-entry TTL
// and total memory cap in bytes (configured via MB in ctor).
//
// Notes:
// - Size accounting is approximate and uses key.size()+value.size().
// - TTL is enforced lazily: on get() of a key, and via prune() on set().
// - Thread-safe via an internal mutex for public methods.

#pragma once

#include <unordered_map>
#include <list>
#include <string>
#include <chrono>
#include <optional>
#include <cstddef>
#include <mutex>



class LRUCache {
public:
    using Clock = std::chrono::steady_clock;

    explicit LRUCache(std::size_t capacity_mb);
    void set(const std::string& key, const std::string& value, std::chrono::milliseconds ttl_ms = std::chrono::seconds(300));
    std::optional<std::string> get(const std::string& key);
    std::optional<std::chrono::milliseconds> ttl_left(const std::string& key);
    bool contains(const std::string& key);
    bool erase(const std::string& key);
    void clear();

    void prune();
    void prune_unlocked();
    std::size_t used_bytes() const;
    std::size_t capacity_bytes() const;
    std::size_t size() const;

private:
    struct Entry {
        std::string key;
        std::string value;
        std::size_t key_size{0};
        std::size_t value_size{0};
        Clock::time_point expire_at;
    };

    struct Node {
        Entry entry;
        std::list<std::string>::iterator lru_it;
    };

    static Clock::time_point no_expire_time();

    static bool is_no_expire(const Entry& e);

    static bool is_expired(const Entry& e);

    void touch(std::list<std::string>::iterator it);

    void evict_over_capacity();

    void erase_it(typename std::unordered_map<std::string, Node>::iterator map_it);

    void erase_it(typename std::unordered_map<std::string, Node>::iterator map_it, std::list<std::string>::iterator lru_it);

private:
    // Тестовый хук: активируется только в сборке с LRU_TEST_HOOK,
    // чтобы сделать предсказуемым срабатывание пограничных условий времени.
    static void test_yield_hook();

    std::size_t capacity_bytes_{};
    std::size_t used_bytes_{};
    std::list<std::string> lru_; // front = most recently used
    std::unordered_map<std::string, Node> map_;
    mutable std::mutex mutex_;
};

/*
Usage example:

#include "LRUCache.h"
using util::LRUCache;

int main() {
    LRUCache cache(1); // 1 MB
    cache.set("a", "12345", std::chrono::seconds(2));
    auto v = cache.get("a"); // has value
    std::this_thread::sleep_for(std::chrono::seconds(3));
    auto v2 = cache.get("a"); // expired -> nullopt
}
*/
