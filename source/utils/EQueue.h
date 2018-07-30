/*-
 * Copyright (c) 2018 TAO Zhijiang<taozhijiang@gmail.com>
 *
 * Licensed under the BSD-3-Clause license, see LICENSE for full information.
 *
 */


#ifndef _TZ_EQUEUE_H_
#define _TZ_EQUEUE_H_

#include <vector>
#include <deque>

#include <mutex>
#include <condition_variable>
#include <chrono>
#include <functional>

template<typename T>
class EQueue {
public:
    EQueue() {}
    virtual ~EQueue() {}

    void PUSH(const T& t){
        std::lock_guard<std::mutex> lock(lock_);
        items_.push_back(t);
        item_notify_.notify_one();
    }

    void POP(T& t) {
        t = POP();
    }

    T POP() {
        std::unique_lock<std::mutex> lock(lock_);
        while (items_.empty()) {
            item_notify_.wait(lock);
        }

        T t = items_.front();
        items_.pop_front();
        return t;
    }

    size_t POP(std::vector<T>& vec, size_t max_count, uint64_t msec) {
        std::unique_lock<std::mutex> lock(lock_);

        while (items_.empty()) {
            if (!item_notify_.wait_for(lock, std::chrono::milliseconds(msec))){
                goto check;
            }
        }
check:
        if (items_.empty()) {
            return 0;
        }

        size_t ret_count = 0;
        do {
            T t = items_.front();
            items_.pop_front();
            vec.emplace_back(t);
            ++ ret_count;
        } while ( ret_count < max_count && !items_.empty());

        return ret_count;
    }

    bool POP(T& t, uint64_t msec) {
        std::unique_lock<std::mutex> lock(lock_);

        // if(!item_notify_.timed_wait(lock, timeout, std::bind(&EQueue::EMPTY, this))){
        while (items_.empty()) {
            if (!item_notify_.wait_for(lock, std::chrono::milliseconds(msec))){
                goto check;
            }
        }

check:
        if (items_.empty()) {
            return false;
        }

        t = items_.front();
        items_.pop_front();
        return true;
    }

    // 只有t不存在的时候才添加
    bool UNIQUE_PUSH(const T& t) {
        std::lock_guard<std::mutex> lock(lock_);
        if (std::find(items_.begin(), items_.end(), t) == items_.end()) {
            items_.push_back(t);
            item_notify_.notify_one();
            return true;
        }
        return false;
    }

    size_t SIZE() {
        std::lock_guard<std::mutex> lock(lock_);
        return items_.size();
    }

    bool EMPTY() {
        std::lock_guard<std::mutex> lock(lock_);
        return items_.empty();
    }

    size_t SHRINK_FRONT(size_t sz) {
        std::lock_guard<std::mutex> lock(lock_);

        size_t orig_sz = items_.size();
        if (orig_sz <= sz)
            return 0;

        auto iter = items_.end() - sz;
        items_.erase(items_.begin(), iter);

        return orig_sz - items_.size();
    }

private:
    std::mutex lock_;
    std::condition_variable item_notify_;

    std::deque<T> items_;
};


#endif // _TZ_EQUEUE_H_

