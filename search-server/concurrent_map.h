#pragma once
#include <map>
#include <vector>
#include <mutex>

using std::operator""s;

template <typename Key, typename Value>
class ConcurrentMap {

private:
    struct Bucket {
        std::mutex mutex; 
        std::map<Key, Value> map;
    };
public:
 static_assert(std::is_integral_v<Key>, "ConcurrentMap supports only integer keys"s);
 

    struct Access {
        std::lock_guard <std::mutex> guard; 
        Value& ref_to_value;
        
        Access(const Key& key, Bucket& bucket)
            : guard(bucket.mutex),
              ref_to_value(bucket.map[key])
        {
        }
    };

    explicit ConcurrentMap(size_t bucket_count):buckets_(bucket_count){
    }
    
    size_t Erase(const Key& key) {
        size_t id = static_cast<uint64_t>(key) % buckets_.size();
        std::lock_guard<std::mutex> guard(buckets_[id].mutex);
        return buckets_[id].map.erase(key);
      
    }

    Access operator[](const Key& key){
    auto& bucket = buckets_[static_cast<uint64_t>(key) % buckets_.size()];
        return { key, bucket };
        }

    std::map<Key, Value> BuildOrdinaryMap() {
        std::map<Key, Value> result;
        for (auto& [mutex, map] : buckets_) {
            std::lock_guard guard(mutex);
            result.insert(map.begin(), map.end());
        }
        return result;
    }
    
    

private:
    
    std::vector<Bucket> buckets_;
};