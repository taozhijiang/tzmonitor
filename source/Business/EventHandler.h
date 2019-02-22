/*-
 * Copyright (c) 2018 TAO Zhijiang<taozhijiang@gmail.com>
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
#include <boost/noncopyable.hpp>

#include <Utils/EQueue.h>
#include <Utils/Log.h>

#include <Business/StoreIf.h>
#include <Business/EventItem.h>


// 服务端的参数配置
struct EventHandlerConf {

    boost::atomic<int> event_linger_;
    boost::atomic<int> event_step_;

    boost::atomic<int> additional_process_queue_size_;
    std::string store_type_;

    EventHandlerConf():
        event_linger_(0),
        event_step_(0),
        additional_process_queue_size_(0),
        store_type_("mysql") {
    }

    // 拷贝操作符
    EventHandlerConf(const EventHandlerConf& other) {
        event_linger_ = other.event_linger_.load();
        event_step_   = other.event_step_.load();
        additional_process_queue_size_ = other.additional_process_queue_size_.load();
        store_type_   = other.store_type_;
    }

    EventHandlerConf& operator=(const EventHandlerConf& other) {
        event_linger_ = other.event_linger_.load();
        event_step_   = other.event_step_.load();
        additional_process_queue_size_ = other.additional_process_queue_size_.load();
        store_type_   = other.store_type_;
    }
};



class EventHandler: public boost::noncopyable,
                    public std::enable_shared_from_this<EventHandler> {
public:
    EventHandler(const std::string& service, const std::string& entity_idx):
        service_(service),
        entity_idx_(entity_idx),
        identity_(construct_identity(service, entity_idx)),
        lock_(),
        conf_(),
        events_(),
        store_() {
    }

    ~EventHandler() {}

    bool init();

public:
    // 添加记录事件
    int add_event(const event_report_t& evs);
    int get_event(const event_cond_t& cond, event_select_t& stat);

private:

    // 正常线程工作函数
    void run();

    // boost 增加线程突发的时候使用
    void run_once_task(std::vector<events_by_time_ptr_t> events);

    // should be called with lock already hold
    int do_add_event(time_t ev_time, const std::vector<event_data_t>& data);

    int do_process_event(events_by_time_ptr_t event, event_insert_t copy_stat);

private:
    const std::string service_;
    const std::string entity_idx_;
    const std::string identity_;

    // 超过linger时间后的事件就会丢到这里被处理
    tzrpc::EQueue<events_by_time_ptr_t> process_queue_;
    std::shared_ptr<boost::thread> thread_ptr_;

    std::mutex lock_;
    EventHandlerConf conf_;

    // 当前
    timed_events_ptr_t events_;

    std::shared_ptr<StoreIf> store_;


};

#endif // _BUSINESS_EVENT_HANDLER_H__
