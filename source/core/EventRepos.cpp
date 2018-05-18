#include <boost/bind.hpp>

#include <utils/Log.h>
#include <utils/Utils.h>

#include "Helper.h"
#include "EventRepos.h"


// EventHandler

bool EventHandler::init() {
    if (identity_.empty()) {
        return false;
    }

    check_timer_id_ = helper::register_timer_task(boost::bind(&EventHandler::check_timer_run, shared_from_this()), 500, true, true);
    if (check_timer_id_ == 0) {
        log_err("Register check_timer failed! ");
        return false;
    }

    thread_ptr_.reset(new boost::thread(boost::bind(&EventHandler::run, shared_from_this())));
    if (!thread_ptr_){
        return false;
    }

    return true;
}


EventHandler::~EventHandler() {

    helper::revoke_timer_task(check_timer_id_);
    check_timer_id_ = -1;

    log_err("EventHandler %s destroied...", identity_.c_str());
}

void EventHandler::check_timer_run() {

    boost::lock_guard<boost::mutex> lock(lock_);

    time_t now = ::time(NULL);
    for (auto iter = events_.begin(); iter != events_.end(); /*nop*/) {
        if (iter->first + EventRepos::instance().get_event_linger() < now ) {

            process_queue_.PUSH(iter->second);

            // References and iterators to the erased elements are invalidated. Other references and iterators are not affected.
            events_.erase(iter++);

        } else {
            break;
        }
    }
}

int EventHandler::add_event(const std::string& identity, const event_report_t& ev) {

    if (identity != identity_) {
        log_err("Error for identity check, expect %s, but get %s", identity_.c_str(), identity.c_str());
        return ErrorDef::ParamErr;
    }

    boost::lock_guard<boost::mutex> lock(lock_);
    time_t now = ::time(NULL);
    if (now - ev.time > EventRepos::instance().get_event_linger()) {
        log_err("Too old report: %s, %ld - %ld", identity.c_str(), now, ev.time);
        return ErrorDef::TimeoutErr;
    }

    return do_add_event(identity, ev.time, ev.data);
}

int EventHandler::do_add_event(const std::string& identity, time_t ev_time, const std::vector<event_data_t>& data) {

    auto time_iter = events_.find(ev_time);
    if (time_iter == events_.end()) {
        log_debug("create time slot: %ld", ev_time);
        events_[ev_time] = std::make_shared<events_t>();
        time_iter = events_.find(ev_time);
    }
    std::shared_ptr<events_t> time_slot_ptr = time_iter->second;

    for (auto iter = data.begin(); iter != data.end(); ++iter) {
        auto event_iter = time_slot_ptr->find(iter->name);
        if (event_iter == time_slot_ptr->end()) {
            log_debug("create event slot: %s", iter->name.c_str());
            (*time_slot_ptr)[iter->name] = std::vector<event_data_t>();
            event_iter = time_slot_ptr->find(iter->name);
        }

        event_iter->second.push_back(*iter);
    }
    return ErrorDef::OK;
}

void calc_event_data(const event_data_t& data, std::map<std::string, size_t>& counts, std::map<std::string, int64_t>& values) {
    if (counts.find(data.flag) == counts.end()) {
        counts[data.flag] = 0;
    }
    counts[data.flag] += 1;

    if (values.find(data.flag) == values.end()) {
        values[data.flag] = 0;
    }
    values[data.flag] += data.value;
}

// process thread
//
// 虽然在handler中直接操作数据库也是可以的，但是一次数据库几十、上百毫秒的延迟会极大的
// 拖慢系统的性能，同时许多进程访问数据库也会导致高并发数据库访问下的各种问题
//

void EventHandler::run() {

    log_info("EventHandler thread begin to run ...");

    while (true) {

        events_ptr_t event;
        if( !process_queue_.POP(event, 5000) ){
            continue;
        }

        // process event
        for (auto iter = event->begin(); iter != event->end(); ++iter) {
            auto& spec_events = iter->second;
            log_debug("process event %s, count %d", iter->first.c_str(), static_cast<int>(spec_events.size()));

            std::map<std::string, size_t> flag_count;
            std::map<std::string, int64_t> flag_value;

            std::for_each(spec_events.begin(), spec_events.end(), boost::bind(calc_event_data, _1, std::ref(flag_count), std::ref(flag_value)));

            log_debug("process reuslt for event: %s", iter->first.c_str());
            for (auto it = flag_count.begin(); it != flag_count.end(); ++it) {
                log_debug("flag %s, count %d, value %ld", it->first.c_str(),
                          static_cast<int>(flag_count[it->first]), flag_value[it->first]);
            }
        }

    }
}




//
// EventRepos


EventRepos& EventRepos::instance() {
    static EventRepos handler {};
    return handler;
}


bool EventRepos::init() {

    int event_linger = 0;

    if (!get_config_value("core.event_linger", event_linger) || event_linger <= 0) {
        return false;
    }

    log_info("We will use event_linger time %d sec.", event_linger);
    config_.event_linger_ = event_linger;
    return true;
}

int EventRepos::destory_handlers() {
    handlers_.clear();
    return ErrorDef::OK;
}


// forward request to specified handlers
int EventRepos::add_event(const event_report_t& evs) {

    if (evs.host.empty() || evs.serv.empty() || evs.entity_idx.empty() || evs.data.empty()) {
        log_err("evs param check error!");
        return ErrorDef::ParamErr;
    }

    std::string identity = construct_identity(evs.host, evs.serv, evs.entity_idx);
    std::shared_ptr<EventHandler> handler;

    if (find_or_create_event_handler(identity, handler) != 0) {
        return ErrorDef::Error;
    }

    return handler->add_event(identity, evs);
}
