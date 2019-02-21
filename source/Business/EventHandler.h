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

using namespace tzrpc;



// 服务端的参数配置
struct EventHandlerConf {

    boost::atomic<int> event_linger_;
    boost::atomic<int> event_step_;

    boost::atomic<int> process_queue_size_;

    EventHandlerConf():
        event_linger_(5),
        event_step_(1),
        process_queue_size_(5) {
    }

};



class EventHandler: public boost::noncopyable,
                    public std::enable_shared_from_this<EventHandler> {
public:
    EventHandler(const std::string& service, const std::string& entity_idx):
        service_(service), entity_idx_(entity_idx),
        identity_(construct_identity(service, entity_idx)) {
    }

    ~EventHandler();

    bool init();

public:
    // 添加记录事件
    int add_event(const event_report_t& evs);

private:

    // 正常线程工作函数
    void run();

    // boost 增加线程突发的时候使用
    void run_once_task(std::vector<events_ptr_t> events);

    // should be called with lock already hold
    int do_add_event(time_t ev_time, const std::vector<event_data_t>& data);

private:
    const std::string service_;
    const std::string entity_idx_;
    const std::string identity_;    // for debug info purpose

    EQueue<events_ptr_t> process_queue_;
    std::shared_ptr<boost::thread> thread_ptr_;

    // 相同 service + entity_idx 会在同一个Handler下面处理
    std::mutex lock_;
    timed_events_ptr_t events_;
};

#endif // _BUSINESS_EVENT_HANDLER_H__
