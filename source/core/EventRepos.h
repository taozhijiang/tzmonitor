#ifndef _TZ_EVENT_REPOS_H__
#define _TZ_EVENT_REPOS_H__

#include "General.h"

#include <deque>
#include <boost/noncopyable.hpp>

#include <utils/EQueue.h>

#include "EventItem.h"

class EventHandler: public boost::noncopyable,
                    public std::enable_shared_from_this<EventHandler> {
public:
    explicit EventHandler(const std::string& identity):
        identity_(identity),
        check_timer_id_(-1) {
    }

    ~EventHandler();

    bool init();

public:
    // 添加记录事件
    int add_event(const std::string& identity, const event_report_t& evs);

private:
    void run();

    // should be called with lock already hold
    int do_add_event(const std::string& identity, time_t ev_time, const std::vector<event_data_t>& data);

private:
    const std::string identity_;

    EQueue<events_ptr_t> process_queue_;

    std::shared_ptr<boost::thread> thread_ptr_;

    boost::mutex lock_;
    timed_events_t events_;

    int64_t check_timer_id_;
    void check_timer_run();
};


struct EventReposConfig {
    time_t event_linger_;
};


class EventRepos: public boost::noncopyable {
public:
    static EventRepos& instance();
    bool init();
    int destory_handlers();

    // forward request to specified handlers
    int add_event(const event_report_t& evs);

    time_t get_event_linger() {
        return config_.event_linger_;
    }

private:

    int find_or_create_event_handler(const std::string& identity, std::shared_ptr<EventHandler>& handler) {

        if (identity.empty()) {
            log_err("identity empty!");
            return ErrorDef::ParamErr;
        }

        { // read lock
            boost::shared_lock<boost::shared_mutex> rlock(rw_lock_);
            auto iter = handlers_.find(identity);
            if (iter!= handlers_.end()) {
                handler = iter->second;
                return ErrorDef::OK;
            }
        }

        { // write lock, double lock check
            boost::unique_lock<boost::shared_mutex> wlock(rw_lock_);
            auto iter = handlers_.find(identity);
            if (iter != handlers_.end()) {
                handler = iter->second;
                return ErrorDef::OK;
            }

            // create new handler
            std::shared_ptr<EventHandler> new_handler;
            if (do_create_event_handler(identity, new_handler) == ErrorDef::OK) {
                handler = new_handler;
                return ErrorDef::OK;
            }
        }

        return ErrorDef::Error;
    }

    // should be call with lock already hold
    int do_create_event_handler(const std::string& identity, std::shared_ptr<EventHandler>& new_handler) {
        std::shared_ptr<EventHandler> handler = std::make_shared<EventHandler>(identity);
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

    boost::shared_mutex rw_lock_;
    std::map<std::string, std::shared_ptr<EventHandler>> handlers_;

    EventReposConfig config_;

private:
    EventRepos(){}
    ~EventRepos(){}
};


#endif // _TZ_EVENT_REPOS_H__
