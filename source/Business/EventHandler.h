/*-
 * Copyright (c) 2019 TAO Zhijiang<taozhijiang@gmail.com>
 *
 * Licensed under the BSD-3-Clause license, see LICENSE for full information.
 *
 */


#ifndef _BUSINESS_EVENT_HANDLER_H__
#define _BUSINESS_EVENT_HANDLER_H__

#include <libconfig.h++>

#include <deque>
#include <mutex>

#include <boost/thread.hpp>

#include <Utils/EQueue.h>
#include <Utils/Log.h>

#include <Business/StoreIf.h>
#include <Business/EventItem.h>

// INTEL Guaranteed Atomic Operations
// Reading or writing a doubleword aligned on a 32-bit boundary
//
// we can check by
//  (int)offsetof(sa, a)
//  (int)offsetof(sa, a)

// 服务端的参数配置
struct EventHandlerConf {

    int event_linger_;
    int event_step_;

    int additional_process_step_size_;

    std::string store_type_;

    EventHandlerConf():
        event_linger_(0),
        event_step_(0),
        additional_process_step_size_(0),
        store_type_("mysql") {
    }

    // 按照event_step_的形式进行时间规约
    time_t nice_step(time_t t) {
        return t + ( event_step_  -  t % event_step_);
    }
} __attribute__ ((aligned (4)));



class EventHandler: public std::enable_shared_from_this<EventHandler> {
public:
    EventHandler(const std::string& service, const std::string& entity_idx):
        service_(service),
        entity_idx_(entity_idx),
        identity_(construct_identity(service, entity_idx)),
        process_queue_(),
        thread_ptr_(),
        thread_terminate_(false),
        conf_(),
        lock_(),
        events_(),
        store_() {
    }

    ~EventHandler() {
        
        thread_terminate_ = true;
        if(thread_ptr_ && thread_ptr_->joinable()) {
            thread_ptr_->join();
        }
    }

    // 禁止拷贝
    EventHandler(const EventHandler&) = delete;
    EventHandler& operator=(const EventHandler&) = delete;

    bool init();

public:
    // 添加记录事件
    int add_event(const event_report_t& evs);
    int get_event(const event_cond_t& cond, event_select_t& stat);
    int get_handler_conf(EventHandlerConf& handler_conf) {
        handler_conf = conf_;
        return 0;
    }

    int update_runtime_conf(const libconfig::Config& conf);
    int module_status(std::string& strModule, std::string& strKey, std::string& strValue);

private:

    // 正常线程工作函数
    void run();

    // boost 增加线程突发的时候使用
    void run_once_task(std::vector<events_by_time_ptr_t> events);

    // should be called with lock already hold
    int do_add_event(time_t ev_time, const std::vector<event_data_t>& data);

    // 大于linger时间之后，就会将信息进行聚合合并操作
    void linger_check_run();
    int do_process_event(events_by_time_ptr_t event, event_insert_t copy_stat);

private:
    const std::string service_;
    const std::string entity_idx_;
    const std::string identity_;

    // 超过linger时间后的事件就会丢到这里被处理
    tzrpc::EQueue<events_by_time_ptr_t> process_queue_;
    std::shared_ptr<boost::thread> thread_ptr_;
    bool thread_terminate_;

    EventHandlerConf conf_;

    // 当前
    std::mutex lock_;
    timed_events_ptr_t events_;

    std::shared_ptr<StoreIf> store_;


};

#endif // _BUSINESS_EVENT_HANDLER_H__
