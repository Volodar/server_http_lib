
#include "LRUCache.h"

#include <thread>

LRUCache::LRUCache(std::size_t capacity_mb)
: capacity_bytes_(capacity_mb * 1024ULL * 1024ULL) {
    
}

// Insert or update a value with TTL. ttl_ms == 0 means no expiration.
void LRUCache::set(const std::string& key, const std::string& value, std::chrono::milliseconds ttl_ms) {
    std::scoped_lock<std::mutex> lock(mutex_);
    prune_unlocked(); // remove expired entries proactively

    std::size_t ksz = key.size();
    std::size_t vsz = value.size();
    const auto exp = ttl_ms.count() == 0 ? no_expire_time() : Clock::now() + ttl_ms;

    auto it = map_.find(key);
    if (it != map_.end()) {
        // Update existing
        Entry& e = it->second.entry;
        // Adjust used bytes for value and key delta
        if (e.key_size != ksz) {
            used_bytes_ -= e.key_size;
            used_bytes_ += ksz;
            e.key_size = ksz;
        }
        used_bytes_ -= e.value_size;
        used_bytes_ += vsz;
        e.value = value;
        e.value_size = vsz;
        e.expire_at = exp;
        touch(it->second.lru_it);
    } else {
        // Insert new
        lru_.push_front(key);
        Node node;
        node.lru_it = lru_.begin();
        node.entry = Entry{key, value, ksz, vsz, exp};
        map_.emplace(key, std::move(node));
        used_bytes_ += ksz + vsz;
    }

    evict_over_capacity();
}

// Get value by key. Returns nullopt if missing or expired.
std::optional<std::string> LRUCache::get(const std::string& key) {
    std::scoped_lock<std::mutex> lock(mutex_);
    auto it = map_.find(key);
    if (it == map_.end())
        return std::nullopt;

    if (is_expired(it->second.entry)) {
        erase_it(it);
        return std::nullopt;
    }

    touch(it->second.lru_it);
    return it->second.entry.value;
}

// Remaining TTL for a key. nullopt if missing or no-expire, or expired (and key removed).
std::optional<std::chrono::milliseconds> LRUCache::ttl_left(const std::string& key) {
    std::scoped_lock<std::mutex> lock(mutex_);
    auto it = map_.find(key);
    if (it == map_.end())
        return std::nullopt;
    if (is_expired(it->second.entry)){
        erase_it(it);
        return std::nullopt;
    }
    if (it->second.entry.expire_at == no_expire_time())
        return std::nullopt;
    // Тестовый хук — помогает стабильно пройти ветку (expire_at <= now)
    test_yield_hook();
    auto now = Clock::now();
    if (it->second.entry.expire_at <= now) {
        erase_it(it);
        return std::nullopt;
    }
    return std::chrono::duration_cast<std::chrono::milliseconds>(it->second.entry.expire_at - now);
}

bool LRUCache::contains(const std::string& key) {
    std::scoped_lock<std::mutex> lock(mutex_);
    auto it = map_.find(key);
    if (it == map_.end()) return false;
    if (is_expired(it->second.entry)) { erase_it(it); return false; }
    return true;
}

bool LRUCache::erase(const std::string& key) {
    std::scoped_lock<std::mutex> lock(mutex_);
    auto it = map_.find(key);
    if (it == map_.end()) return false;
    erase_it(it);
    return true;
}

void LRUCache::clear() {
    std::scoped_lock<std::mutex> lock(mutex_);
    map_.clear();
    lru_.clear();
    used_bytes_ = 0;
}

// Manually remove expired entries.
void LRUCache::prune() {
    std::scoped_lock<std::mutex> lock(mutex_);
    prune_unlocked();
}

// Unsafe variant; caller must hold mutex_.
void LRUCache::prune_unlocked() {
    if (map_.empty())
        return;
    // Iterate safely through keys via LRU list to allow O(1) erase
    for (auto it = lru_.begin(); it != lru_.end(); ) {
        auto map_it = map_.find(*it);
        if (map_it != map_.end() && is_expired(map_it->second.entry)) {
            auto to_erase = it++;
            erase_it(map_it, to_erase);
        } else {
            ++it;
        }
    }
}

std::size_t LRUCache::used_bytes() const {
    std::scoped_lock<std::mutex> lock(mutex_);
    return used_bytes_;
}
std::size_t LRUCache::capacity_bytes() const {
    return capacity_bytes_;
}
std::size_t LRUCache::size() const {
    std::scoped_lock<std::mutex> lock(mutex_);
    return map_.size();
}

LRUCache::Clock::time_point LRUCache::no_expire_time() {
    return Clock::time_point::max();
}

bool LRUCache::is_no_expire(const Entry& e) {
    return e.expire_at == no_expire_time();
}

bool LRUCache::is_expired(const Entry& e) {
    return !is_no_expire(e) && Clock::now() >= e.expire_at;
}

void LRUCache::touch(std::list<std::string>::iterator it) {
    if (it != lru_.begin()) {
        lru_.splice(lru_.begin(), lru_, it);
    }
}

void LRUCache::evict_over_capacity() {
    // Remove expired from back first if any
    while (used_bytes_ > capacity_bytes_ && !lru_.empty()) {
        auto back_it = std::prev(lru_.end());
        auto map_it = map_.find(*back_it);
        if (map_it == map_.end()) {
            lru_.erase(back_it);
            continue;
        }
        erase_it(map_it, back_it);
    }
}

// Erase using map iterator only (finds LRU iterator)
void LRUCache::erase_it(typename std::unordered_map<std::string, Node>::iterator map_it) {
    used_bytes_ -= (map_it->second.entry.key_size + map_it->second.entry.value_size);
    lru_.erase(map_it->second.lru_it);
    map_.erase(map_it);
}

// Erase using both iterators to avoid second lookup
void LRUCache::erase_it(typename std::unordered_map<std::string, Node>::iterator map_it, std::list<std::string>::iterator lru_it) {
    used_bytes_ -= (map_it->second.entry.key_size + map_it->second.entry.value_size);
    lru_.erase(lru_it);
    map_.erase(map_it);
}

void LRUCache::test_yield_hook() {
#ifdef LRU_TEST_HOOK
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
#endif
}
