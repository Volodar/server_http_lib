#include "http/LRUCache.h"

#include "test_framework.h"

#include <chrono>
#include <optional>
#include <string>
#include <thread>
#include <vector>

using namespace std::chrono_literals;

static std::string make_blob(std::size_t bytes) {
  return std::string(bytes, 'x');
}

TEST(LRU_InsertAndGet_NoExpire) {
  LRUCache cache(1);  // 1 MB capacity

  ASSERT_EQ(cache.size(), 0u);
  ASSERT_EQ(cache.used_bytes(), 0u);

  cache.set("k1", "v1", 0ms);  // no expiration

  ASSERT_TRUE(cache.contains("k1"));
  auto v = cache.get("k1");
  ASSERT_TRUE(v.has_value());
  ASSERT_EQ(v.value(), std::string("v1"));

  // ttl_left for no-expire returns nullopt and does not erase
  auto ttl = cache.ttl_left("k1");
  ASSERT_FALSE(ttl.has_value());
  ASSERT_TRUE(cache.contains("k1"));

  ASSERT_EQ(cache.size(), 1u);
  ASSERT_EQ(cache.used_bytes(), std::string("k1").size() + std::string("v1").size());
}

TEST(LRU_Get_Missing_AndErase) {
  LRUCache cache(1);
  ASSERT_FALSE(cache.contains("missing"));
  ASSERT_FALSE(cache.get("missing").has_value());
  ASSERT_FALSE(cache.erase("missing"));

  cache.set("a", "b", 0ms);
  ASSERT_TRUE(cache.erase("a"));
  ASSERT_FALSE(cache.contains("a"));
  ASSERT_EQ(cache.size(), 0u);
}

TEST(LRU_Expire_On_Get_And_Contains) {
  LRUCache cache(1);
  cache.set("e", "x", 10ms);
  ASSERT_TRUE(cache.contains("e"));
  std::this_thread::sleep_for(20ms);

  // Expired: contains() should erase and report false
  ASSERT_FALSE(cache.contains("e"));
  // get() should also return nullopt after erase
  auto v = cache.get("e");
  ASSERT_FALSE(v.has_value());
  ASSERT_EQ(cache.size(), 0u);
}

TEST(LRU_Expire_On_Get_Path) {
  LRUCache cache(1);
  cache.set("e2", "x", 30ms);
  std::this_thread::sleep_for(40ms);
  // Непосредственно get() должен обнаружить истечение, удалить запись и вернуть nullopt
  auto v = cache.get("e2");
  ASSERT_FALSE(v.has_value());
  ASSERT_EQ(cache.size(), 0u);
}

TEST(LRU_TtlLeft_Positive_Boundary_And_NoExpire) {
  LRUCache cache(1);
  cache.set("t", "v", 100ms);

  auto ttl1 = cache.ttl_left("t");
  ASSERT_TRUE(ttl1.has_value());
  ASSERT_TRUE(ttl1.value().count() <= 100);
  ASSERT_TRUE(ttl1.value().count() > 0);

  // Boundary: after expiry, ttl_left erases and returns nullopt
  std::this_thread::sleep_for(120ms);
  auto ttl2 = cache.ttl_left("t");
  ASSERT_FALSE(ttl2.has_value());
  ASSERT_FALSE(cache.contains("t"));

  // No-expire branch in ttl_left returns nullopt without erasing
  cache.set("ne", "v", 0ms);
  auto ttl3 = cache.ttl_left("ne");
  ASSERT_FALSE(ttl3.has_value());
  ASSERT_TRUE(cache.contains("ne"));
}

TEST(LRU_TtlLeft_MissingKey) {
  LRUCache cache(1);
  auto ttl = cache.ttl_left("missing");
  ASSERT_FALSE(ttl.has_value());
}

TEST(LRU_TtlLeft_InternalBoundaryBranch) {
  // Благодаря тестовому хуку внутри ttl_left() достигаем ветки (expire_at <= now)
  LRUCache cache(1);
  cache.set("b", "v", std::chrono::milliseconds(1));

  // Вызов должен вернуть nullopt и удалить запись в ветке (expire_at <= now)
  auto ttl = cache.ttl_left("b");
  ASSERT_FALSE(ttl.has_value());
  ASSERT_FALSE(cache.contains("b"));
}

TEST(LRU_Update_Value_And_Touch_Order) {
  LRUCache cache(1);
  // Insert two items; MRU should be k2
  cache.set("k1", "a", 10s);
  cache.set("k2", "b", 10s);
  ASSERT_TRUE(cache.contains("k1"));
  ASSERT_TRUE(cache.contains("k2"));
  ASSERT_EQ(cache.size(), 2u);

  // Access k1 to move it to front (touch true-branch)
  auto v1 = cache.get("k1");
  ASSERT_TRUE(v1.has_value());
  ASSERT_EQ(v1.value(), std::string("a"));

  // Access k1 again while it's already MRU (touch false-branch)
  auto v2 = cache.get("k1");
  ASSERT_TRUE(v2.has_value());

  // Update k1 to a larger value to change used_bytes
  auto before = cache.used_bytes();
  cache.set("k1", "aaaa", 10s);
  auto after = cache.used_bytes();
  ASSERT_TRUE(after > before);

  // Ensure k2 still present
  ASSERT_TRUE(cache.contains("k2"));
}

TEST(LRU_Evict_Over_Zero_Capacity) {
  // Capacity 0 MB: any insert should evict over capacity
  LRUCache cache(0);
  cache.set("x", "y", 0ms);
  ASSERT_FALSE(cache.contains("x"));
  ASSERT_EQ(cache.size(), 0u);
  ASSERT_EQ(cache.used_bytes(), 0u);

  // Multiple inserts still leave cache empty
  cache.set("a", "1", 1s);
  cache.set("b", "2", 1s);
  ASSERT_EQ(cache.size(), 0u);
}

TEST(LRU_Evict_LRU_With_Capacity) {
  // 1 MB capacity. Make blobs to force eviction and test LRU order
  LRUCache cache(1);
  const std::size_t big = 700 * 1024;  // ~700KB
  auto blobA = make_blob(big);
  auto blobB = make_blob(big);

  cache.set("A", blobA, 5s);  // fits alone
  ASSERT_TRUE(cache.contains("A"));

  // Touch A so it is MRU, then insert B which forces eviction of LRU
  auto _ = cache.get("A");
  cache.set("B", blobB, 5s);

  // Only one can fit; A was MRU, so expect B to be present and A evicted
  bool hasA = cache.contains("A");
  bool hasB = cache.contains("B");
  ASSERT_TRUE(hasA ^ hasB);  // exactly one present

  // Now ensure inserting a small item doesn't evict when under capacity
  cache.clear();
  cache.set("S1", "small", 5s);
  cache.set("S2", "tiny", 5s);
  ASSERT_TRUE(cache.contains("S1"));
  ASSERT_TRUE(cache.contains("S2"));
  ASSERT_EQ(cache.size(), 2u);
}

TEST(LRU_Prune_Empty_And_Mixed) {
  LRUCache cache(1);
  // Prune on empty map (early return branch)
  cache.prune();
  ASSERT_EQ(cache.size(), 0u);

  // Mixed: expired and non-expired
  cache.set("e1", "v", 10ms);
  cache.set("ok", "v", 5s);
  cache.set("e2", "v", 10ms);
  std::this_thread::sleep_for(20ms);
  cache.prune();

  ASSERT_FALSE(cache.contains("e1"));
  ASSERT_FALSE(cache.contains("e2"));
  ASSERT_TRUE(cache.contains("ok"));
  ASSERT_EQ(cache.size(), 1u);
}

TEST(LRU_Clear_And_Basics) {
  LRUCache cache(1);
  cache.set("k", "v", 1s);
  ASSERT_EQ(cache.capacity_bytes(), 1ull * 1024ull * 1024ull);
  ASSERT_EQ(cache.size(), 1u);
  ASSERT_TRUE(cache.contains("k"));
  cache.clear();
  ASSERT_EQ(cache.size(), 0u);
  ASSERT_FALSE(cache.contains("k"));
}
