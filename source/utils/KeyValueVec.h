#ifndef _TZ_KEY_VALUE_VEC_H__
#define _TZ_KEY_VALUE_VEC_H__

// 不同于map的key value，使用vector保持推入的顺序

#include <utility>
#include <boost/function.hpp>

#include "Log.h"

template<typename K, typename V>
class KeyValueVec {
public:
    typedef std::pair<K, V> Entry;
    typedef std::vector<Entry> Container;
    typedef typename Container::iterator iterator;
    typedef typename Container::const_iterator const_iterator;

public:
    KeyValueVec():
        lock_(), items_() {
    }

    ~KeyValueVec() {
    }

public:

    // so called iterator delegation...
    iterator BEGIN() {
        return items_.begin();
    }

    iterator END() {
        return items_.end();
    }

    void PUSH_BACK(const K& k, const V& v) {
        boost::unique_lock<boost::mutex> lock(lock_);
        items_.push_back({k, v}); // make_pair can not use - const ref
    }

    void PUSH_BACK(const Entry& entry) {
        boost::unique_lock<boost::mutex> lock(lock_);
        items_.push_back(entry);
    }

    bool FIND(const K& k, V& v) const {
        boost::unique_lock<boost::mutex> lock(lock_);
        for (size_t idx = 0; idx < items_.size(); ++idx) {
            if (items_[idx].first == k) {
                v = items_[idx].second;
                return true;
            }
        }
        return false;
    }

    bool FIND(const K& k, Entry& entry) const {
        boost::unique_lock<boost::mutex> lock(lock_);
        for (size_t idx = 0; idx < items_.size(); ++idx) {
            if (items_[idx].first == k) {
                entry = items_[idx];
                return true;
            }
        }
        return false;
    }

    size_t SIZE() const {
        boost::unique_lock<boost::mutex> lock(lock_);
        return items_.size();
    }

    bool EMPTY() const {
        boost::unique_lock<boost::mutex> lock(lock_);
        return items_.empty();
    }

    void CLEAR() {
        boost::unique_lock<boost::mutex> lock(lock_);
        items_.clear();
    }

private:
    mutable boost::mutex lock_;
    std::vector<std::pair<K, V> > items_;
};



#endif // _TZ_KEY_VALUE_VEC_H__

