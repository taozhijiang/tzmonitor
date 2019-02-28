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

#include <Utils/EQueue.h>
#include <Utils/TinyTask.h>
#include <Utils/Log.h>

#include <Scaffold/ConfHelper.h>
#include <Business/EventItem.h>
#include <Business/StoreIf.h>


//
// 类似于做一个handler的事件处理分发

class EventHandler;
struct EventHandlerConf;

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

    // key:service
    int get_metrics(const std::string& version, const std::string& service,
                    std::vector<std::string>& metric_stat);
    int get_services(const std::string& version, std::vector<std::string>& service_stat);
    int get_service_conf(const std::string& service, EventHandlerConf& handler_conf);

    void add_additional_task(const tzrpc::TaskRunnable& func) {
        support_task_helper_->add_additional_task(func);
    }

private:


    std::mutex lock_;
    // key service&entity_idx
    typedef std::map<std::string, std::shared_ptr<EventHandler>> HandlerType;
    std::shared_ptr<HandlerType> handlers_;

    // 选取任务的任何一个entity的service handler
    int find_event_handler(const std::string& service,
                           std::shared_ptr<EventHandler>& handler);

    int find_create_event_handler(const std::string& service, const std::string& entity_idx,
                                  std::shared_ptr<EventHandler>& handler);

    // 额外处理线程组，用于辅助增强处理能力
    int support_process_task_size_;  // 目前不支持动态
    std::shared_ptr<tzrpc::TinyTask> support_task_helper_;

    EventHandlerConf get_default_handler_conf();
    std::shared_ptr<EventHandlerConf> default_handler_conf_;


private:
    EventRepos():
        lock_(),
        handlers_(),
        support_process_task_size_(1),
        support_task_helper_(),
        default_handler_conf_() {
    }

    ~EventRepos(){
    }
};


#endif // _BUSINESS_EVENT_REPOS_H__
