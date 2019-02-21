/*-
 * Copyright (c) 2018 TAO Zhijiang<taozhijiang@gmail.com>
 *
 * Licensed under the BSD-3-Clause license, see LICENSE for full information.
 *
 */

#include <xtra_rhel6.h>
#include <functional>

#include <Utils/Log.h>

#include <Business/EventHandler.h>
#include <Business/EventRepos.h>

extern int stateless_process_event(events_ptr_t event, event_insert_t copy_stat);

// EventHandler

bool EventHandler::init() {

    if (identity_.empty()) {
        return false;
    }

    thread_ptr_.reset(new boost::thread(std::bind(&EventHandler::run, shared_from_this())));
    if (!thread_ptr_){
        log_err("create work thread failed! ");
        return false;
    }

    return true;
}


EventHandler::~EventHandler() {
    log_err("EventHandler %s destroied...", identity_.c_str());
}


int EventHandler::add_event(const event_report_t& ev) {

    if (ev.service != service_ || ev.entity_idx != entity_idx_) {
        log_err("Error for identity check, expect (%s, %s), but get (%s, %s)",
                service_.c_str(), entity_idx_.c_str(),
                ev.entity_idx.c_str(), ev.entity_idx.c_str());
        return -1;
    }

    std::lock_guard<std::mutex> lock(lock_);
    time_t now = ::time(NULL);
    if (now - ev.timestamp > EventRepos::instance().get_event_linger()) {
        log_err("Too old report: %s %ld - %ld, drop it!", identity_.c_str(), now, ev.timestamp);
        return -1;
    }

    return do_add_event(ev.timestamp, ev.data);
}

int EventHandler::do_add_event(time_t ev_time, const std::vector<event_data_t>& data) {

    auto timed_iter = events_.find(ev_time);
    if (timed_iter == events_.end()) {
        log_debug("create new time slot: %ld", ev_time);
        events_[ev_time] = std::make_shared<events_t>(ev_time);
        timed_iter = events_.find(ev_time);
    }

    SAFE_ASSERT(timed_iter != events_.end());
    std::map<std::string, std::vector<event_data_t>>& timed_slot = timed_iter->second->data_;

    for (auto iter = data.begin(); iter != data.end(); ++iter) {
        std::map<std::string, std::vector<event_data_t>>::iterator event_iter = timed_slot.find(iter->metric);
        if (event_iter == timed_slot.end()) {
            log_debug("create new event slot: %s in %s@%ld", iter->metric.c_str(), identity_.c_str(), ev_time);
            timed_slot[iter->metric] = std::vector<event_data_t>();
            event_iter = timed_slot.find(iter->metric);
        }

        event_iter->second.push_back(*iter);
    }
    return 0;
}




void EventHandler::run_once_task(std::vector<events_ptr_t> events) {

    log_debug("MonitorEventHandler run_once_task thread %#lx begin to run ...", (long)pthread_self());

    for(auto iter = events.begin(); iter != events.end(); ++iter) {

        event_insert_t stat {};
        stat.service = service_;
        stat.entity_idx = entity_idx_;
        stat.timestamp = (*iter)->timestamp_;

        stateless_process_event(*iter, stat);
    }
}

// process thread
//
// 虽然在handler中直接操作数据库也是可以的，但是一次数据库几十、上百毫秒的延迟会极大的
// 拖慢系统的性能，同时许多进程访问数据库也会导致高并发数据库访问下的各种问题
//

void EventHandler::run() {

    log_info("EventHandler thread %#lx begin to run ...", (long)pthread_self());

    event_insert_t stat {};
    stat.service = service_;
    stat.entity_idx = entity_idx_;

    while (true) {

        int process_queue_size = EventRepos::instance().conf_.process_queue_size_;
        while (process_queue_.SIZE() > 2 * process_queue_size) {
            std::vector<events_ptr_t> ev_inserts;
            size_t ret = process_queue_.POP(ev_inserts, process_queue_size, 5000);
            if (!ret) {
                log_err("Pop support task failed.");
                break;
            }

            std::function<void()> func = std::bind(&EventHandler::run_once_task, shared_from_this(), ev_inserts);
            EventRepos::instance().add_additional_task(func);
        }

        events_ptr_t event;
        if( !process_queue_.POP(event, 5000) ){
            continue;
        }

        stat.service = service_;
        stat.entity_idx = entity_idx_;
        stat.timestamp = event->timestamp_;

        // do actual handle
        stateless_process_event(event, stat);
    }
}

