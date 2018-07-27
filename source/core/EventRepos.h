#ifndef _TZ_EVENT_REPOS_H__
#define _TZ_EVENT_REPOS_H__

#include "General.h"

#include <libconfig.h++>

#include <deque>
#include <mutex>

#include <boost/noncopyable.hpp>
#include <boost/atomic/atomic.hpp>

#include <utils/EQueue.h>
#include <utils/TinyTask.h>
#include <utils/Log.h>

#include "EventItem.h"

class EventHandler: public boost::noncopyable,
                    public std::enable_shared_from_this<EventHandler> {
public:
    EventHandler(const std::string& host, const std::string& serv, const std::string& entity_idx):
        host_(host), serv_(serv), entity_idx_(entity_idx),
        identity_(construct_identity(host, serv, entity_idx)),
        check_timer_id_(-1) {
    }

    ~EventHandler();

    bool init();

public:
    // 添加记录事件
    int add_event(const event_report_t& evs);

private:
    void run();
    void run_once_task(std::vector<events_ptr_t> events);

    // should be called with lock already hold
    int do_add_event(time_t ev_time, const std::vector<event_data_t>& data);

private:
    const std::string host_;
    const std::string serv_;
    const std::string entity_idx_;
    const std::string identity_;    // for debug info purpose

    EQueue<events_ptr_t> process_queue_;
    std::shared_ptr<std::thread> thread_ptr_;

    std::mutex lock_;
    timed_events_ptr_t events_;

    int64_t check_timer_id_;
    void check_timer_run();

    int srv_il_process_queue_size_;
};


struct EventReposConfig {

    boost::atomic<time_t> event_linger_;

    boost::atomic<int> srv_il_process_queue_size_;
    boost::atomic<int> srv_ob_process_task_size_;
};


class EventRepos: public boost::noncopyable {
public:
    static EventRepos& instance();
    bool init();
    int update_run_cfg(const libconfig::Config& cfg);
    int destory_handlers();

    // forward request to specified handlers
    int add_event(const event_report_t& evs);
    int get_event(const event_cond_t& cond, event_query_t& stat);

    time_t get_event_linger() {
        return config_.event_linger_;
    }

    void add_task(const TaskRunnable& func) {
        task_helper_->add_task(func);
    }

private:

    int find_or_create_event_handler(const event_report_t& evs, std::shared_ptr<EventHandler>& handler) {

        if (evs.host.empty() || evs.serv.empty() || evs.entity_idx.empty()) {
            log_err("required identity info empty!");
            return ErrorDef::ParamErr;
        }

        std::string identity = construct_identity(evs.host, evs.serv, evs.entity_idx);
        std::lock_guard<std::mutex> lock(lock_);

        auto iter = handlers_.find(identity);
        if (iter != handlers_.end()) {
            handler = iter->second;
            return ErrorDef::OK;
        }

        // create new handler
        std::shared_ptr<EventHandler> new_handler;
        if (do_create_event_handler(evs, new_handler) == ErrorDef::OK) {
            handler = new_handler;
            return ErrorDef::OK;
        }

        return ErrorDef::Error;
    }

    // should be call with lock already hold
    int do_create_event_handler(const event_report_t& evs, std::shared_ptr<EventHandler>& new_handler) {

        std::string identity = construct_identity(evs.host, evs.serv, evs.entity_idx);
        std::shared_ptr<EventHandler> handler = std::make_shared<EventHandler>(evs.host, evs.serv, evs.entity_idx);
        if (!handler) {
            log_err("Create handler %s failed!", identity.c_str());
            return ErrorDef::CreateErr;
        }

        if (!handler->init()) {
            log_err("Init handler %s failed!", identity.c_str());
            return ErrorDef::InitErr;
        }

        handlers_[identity] = handler;
        new_handler = handler;
        return ErrorDef::OK;
    }

    std::mutex lock_;
    std::map<std::string, std::shared_ptr<EventHandler>> handlers_;

    EventReposConfig config_;

    std::shared_ptr<TinyTask> task_helper_;

private:
    EventRepos(){}
    ~EventRepos(){}
};


#endif // _TZ_EVENT_REPOS_H__
