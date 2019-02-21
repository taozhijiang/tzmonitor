/*-
 * Copyright (c) 2018 TAO Zhijiang<taozhijiang@gmail.com>
 *
 * Licensed under the BSD-3-Clause license, see LICENSE for full information.
 *
 */


#ifndef _BUSINESS_EVENT_REPOS_H__
#define _BUSINESS_EVENT_REPOS_H__

#include <libconfig.h++>

#include <deque>
#include <mutex>

#include <boost/noncopyable.hpp>
#include <boost/atomic/atomic.hpp>

#include <Utils/EQueue.h>
#include <Utils/TinyTask.h>
#include <Utils/Log.h>

#include <Scaffold/ConfHelper.h>
#include <Business/EventItem.h>
#include <Business/StoreIf.h>


//
// 类似于做一个handler的事件处理分发

class EventHandler;

class EventRepos: public boost::noncopyable {

    friend class EventHandler;

public:
    static EventRepos& instance();

    bool init();
    int destory_handlers();

    int update_runtime_conf(const libconfig::Config& conf);
    int module_status(std::string& strModule, std::string& strKey, std::string& strValue);


    // forward request to specified handlers
    int add_event(const event_report_t& evs);
    int get_event(const event_cond_t& cond, event_select_t& stat);
    int get_metrics();

    int get_event_linger() {
        return conf_.event_linger_;
    }

    void add_additional_task(const tzrpc::TaskRunnable& func) {
        task_helper_->add_additional_task(func);
    }

    std::unique_ptr<StoreIf>& store() {
        return store_;
    }

private:


    std::mutex lock_;
    std::map<std::string, std::shared_ptr<EventHandler>> handlers_;

    int find_or_create_event_handler(const event_report_t& evs, std::shared_ptr<EventHandler>& handler);

    // should be call with lock already hold
    int do_create_event_handler(const event_report_t& evs, std::shared_ptr<EventHandler>& new_handler);

    // 额外处理线程组，用于辅助增强处理能力
    int support_task_size_;  // 目前不支持动态
    std::shared_ptr<tzrpc::TinyTask> support_task_helper_;

private:
    EventRepos():
        lock_(),
        handlers_(),
        support_task_size_(1),
        support_task_helper_() {
    }

    ~EventRepos(){}
};


#endif // _BUSINESS_EVENT_REPOS_H__
