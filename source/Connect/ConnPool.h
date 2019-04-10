/*-
 * Copyright (c) 2019 TAO Zhijiang<taozhijiang@gmail.com>
 *
 * Licensed under the BSD-3-Clause license, see LICENSE for full information.
 *
 */


#ifndef _CONNECT_POOL_H_
#define _CONNECT_POOL_H_

#include <xtra_rhel.h>

#include <set>
#include <deque>
#include <numeric>

#include <boost/circular_buffer.hpp>

#include <condition_variable>
#include <chrono>

#include <Utils/Log.h>
#include <Utils/Timer.h>

#include <Scaffold/Status.h>

namespace tzrpc {

template <typename T>
struct conn_ptr_compare {
public:
    typedef std::shared_ptr<T> ConnPtr;

    bool operator() (const ConnPtr& lhs,
                     const ConnPtr& rhs) const {
        return (lhs.get() < rhs.get());
    }
};


struct ConnStat {

    uint32_t start_;             // touch

    void touch() {
        start_ = static_cast<uint32_t>( ::time(NULL) & 0xFFFFFFFFL);
    }

    bool expire(uint32_t linger) {
        time_t elapse = ::time(NULL) - start_;
        return static_cast<uint32_t>( ::time(NULL) & 0xFFFFFFFFL) > linger;
    }

    ConnStat():
        start_() {
        touch();
    }
}  __attribute__ ((aligned (4))) ;


struct ConnPoolStat {
    uint32_t acquired_count_;    // 总请求计数
    uint32_t acquired_success_;  // 成功请求的数量

    void incr_count() {
        ++ acquired_count_;
    }

    void incr_success() {
        ++ acquired_success_;
    }

    ConnPoolStat():
        acquired_count_(0),
        acquired_success_(0) {
    }
}  __attribute__ ((aligned (4)));


template <typename T, typename Helper>
class ConnPool: public std::enable_shared_from_this<ConnPool<T, Helper> >
{
public:
    typedef std::shared_ptr<T> ConnPtr;
    typedef std::weak_ptr<T>   ConnWeakPtr;
    typedef std::set<ConnPtr, conn_ptr_compare<T> > ConnContainer;

public:
    explicit ConnPool(std::string pool_name, size_t capacity, Helper helper,
                      uint32_t linger_sec = 0):
        pool_name_(pool_name), capacity_(capacity),
        helper_(helper),
        conns_busy_(), conns_idle_(),
        conn_notify_(), conn_notify_mutex_(),
        conn_trim_linger_(linger_sec),
        stat_() {

        SAFE_ASSERT(capacity_);
        log_info( "ConnPool Maxium Capacity: %lu", capacity_ );
        return;
    }

    virtual ~ConnPool() {
        log_info("ConnPool Destructed, name: %s, capacity: %lu",
                 pool_name_.c_str(), capacity_);
    }

    // 禁止拷贝
    ConnPool(const ConnPool&) = delete;
    ConnPool& operator=(const ConnPool&) = delete;


    // 增加连接类型的测试，保证在服务启动最开始的时候
    // 就能够侦测出配置、连接的异常情况
    bool init() {

        ConnPtr scoped_ptr {};
        if (!request_scoped_conn(scoped_ptr)) {
            log_err("initialized request scoped test failed.");
            return false;
        }

        if (!scoped_ptr->ping_test()) {
            log_err("initialized ping test failed.");
            return false;
        }

        if (conn_trim_linger_) {
            log_info("we will try to trim idle connection %u sec for %s.",
                     conn_trim_linger_, pool_name_.c_str());

            Timer::instance().add_timer(std::bind(&ConnPool::do_conn_linger_trim, this->shared_from_this()), 1*1000, true);
        }

        Status::instance().register_status_callback(
            "ConnPool_" + pool_name_,
            std::bind(&ConnPool::module_status, this->shared_from_this(),
                      std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

        return true;
    }

    // 由于会返回nullptr，所以不能返回引用
    ConnPtr request_conn() {

        stat_.incr_count();
        std::unique_lock<std::mutex> lock(conn_notify_mutex_);

        while (!do_check_available()) {
            conn_notify_.wait(lock);
        }

        return do_request_conn();
    }

    ConnPtr try_request_conn(size_t msec)  {

        ConnPtr conn;
        stat_.incr_count();
        std::unique_lock<std::mutex> lock(conn_notify_mutex_);

        if (!do_check_available() && !msec)
            return conn; // nullptr

        // timed_wait not work with 0
        if(do_check_available() || conn_notify_.wait_for(lock, std::chrono::milliseconds(msec))) {
            typename ConnContainer::iterator it;
            return do_request_conn();
        }

        return conn;
    }

    bool request_scoped_conn(ConnPtr& scope_conn) {

        // reset first, all will stack at reset latter...
        // probably recursive require conn_nofity_mutex problem

        stat_.incr_count();
        scope_conn.reset();
        std::unique_lock<std::mutex> lock(conn_notify_mutex_);

        while (!do_check_available()) {
            conn_notify_.wait(lock);
        }

        ConnPtr conn = do_request_conn();
        if (conn) {
            scope_conn.reset(conn.get(),
                             std::bind(&ConnPool::free_conn,
                             this, conn)); // 还是通过智能指针拷贝一份吧
            return true;
        }

        return false;
    }

    void free_conn(ConnPtr conn) {

        {
            std::lock_guard<std::mutex> lock(conn_notify_mutex_);

            // 如果健康，则将其丢回连接池中，否则直接丢弃
            if (conn->is_health()) {
                conns_idle_.push_back(conn);
            } else {
                log_err("connect not ok, drop it away");
            }

            conns_busy_.erase(conn);
        }

        conn_notify_.notify_all();
        return;
    }


    size_t get_conn_capacity() const {
        return capacity_;
    }

private:

    // 持锁被调用的 conn_notify_mutex_
    ConnPtr do_request_conn() {

        ConnPtr conn;

        if (!conns_idle_.empty()){
            conn = conns_idle_.front();
            conns_idle_.pop_front();
            conns_busy_.insert(conn);
            stat_.incr_success();
            return conn;
        }

        if ( (conns_idle_.size() + conns_busy_.size()) < capacity_) {

            ConnPtr new_conn = std::make_shared<T>(*this, helper_);
            if (!new_conn){
                log_err("creating new Conn failed!");
                return new_conn;
            }

            if (!new_conn->init(reinterpret_cast<int64_t>(new_conn.get()))) {
                log_err("init new Conn failed!");
                new_conn.reset();
                return new_conn;
            }

            conns_busy_.insert(new_conn);
            stat_.incr_success();
            return new_conn;
        }

        return conn;
    }

    bool do_check_available() {
        return (!conns_idle_.empty() || (conns_idle_.size() + conns_busy_.size()) < capacity_ );
    }


    std::string pool_name_;
    size_t capacity_;       // 总连接数据限制


    // 各种连接类型的配置信息会放在这个模板类型中
    const Helper helper_;

    // 正在被使用的连接
    std::set<ConnPtr, conn_ptr_compare<T> > conns_busy_;
    std::deque<ConnPtr> conns_idle_;

    // If the lock is std::unique_lock, std::condition_variable may provide better performance.
    // http://en.cppreference.com/w/cpp/thread/condition_variable
    std::condition_variable conn_notify_;
    std::mutex conn_notify_mutex_;

    ConnPoolStat stat_;
    const uint32_t conn_trim_linger_;    // 连接闲置该时长之后会被自动删除

private:
    void do_conn_linger_trim() {

        struct timeval start_time {};
        ::gettimeofday(&start_time, NULL);

        std::lock_guard<std::mutex> lock(conn_notify_mutex_);

        if (conns_idle_.empty()){
            return;
        }

        int count = 0;
        int trim_count = 0;

        for (auto iter = conns_idle_.begin(); iter != conns_idle_.end(); ) {

            ++ count;

            // gettimeofday的秒和time的秒是一样的
            if ((*iter)->expire(conn_trim_linger_)) {
                iter = conns_idle_.erase(iter);
                ++ trim_count;
            } else {
                ++ iter;
            }

            if ((count % 20) == 0) {  // 不能卡顿太长时间
                struct timeval now;
                ::gettimeofday(&now, NULL);
                int64_t elapse_ms = ( 1000000 * ( now.tv_sec - start_time.tv_sec ) + now.tv_usec - start_time.tv_usec ) / 1000;
                if (elapse_ms > 20) {
                    log_notice("too long elapse time: %ld ms, break now", elapse_ms);
                    break;
                }
            }
        }

        if (trim_count) {
            log_debug("pool %s, total checked %d conns, trimed %d conns.", pool_name_.c_str(), count, trim_count);
        }

        return;
    }

    int module_status(std::string& module, std::string& name, std::string& val) {

        module = "ConnPool";
        name = pool_name_;

        std::stringstream ss;

        ss << "\t" << "capacity: " << capacity_ << std::endl;
        ss << "\t" << "acquire_count: " << stat_.acquired_count_ << std::endl;
        ss << "\t" << "acquire_success:" << stat_.acquired_success_ << std::endl;

        {
            std::lock_guard<std::mutex> lock(conn_notify_mutex_);
            ss << "\t" << "current_busy:" << conns_busy_.size() << std::endl;
            ss << "\t" << "current_idle:" << conns_idle_.size() << std::endl;
        }

        val = ss.str();

        return 0;
    }

};

} // end namespace tzrpc

#endif  // _CONNECT_POOL_H_
