#include <boost/bind.hpp>

#include <utils/Log.h>
#include <utils/Utils.h>

#include "Helper.h"
#include "EventRepos.h"

#include "EventSql.h"

// EventHandler

bool EventHandler::init() {
    if (identity_.empty()) {
        return false;
    }

    check_timer_id_ =
        helper::register_timer_task(boost::bind(&EventHandler::check_timer_run, shared_from_this()), 500, true, true);
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

            // References and iterators to the erased elements are invalidated.
            // Other references and iterators are not affected.
            events_.erase(iter++);

        } else {
            break;
        }
    }
}

int EventHandler::add_event(const event_report_t& ev) {

    if (ev.host != host_ || ev.serv != serv_ || ev.entity_idx != entity_idx_) {
        log_err("Error for identity check, expect (%s, %s, %s), but get (%s, %s, %s)",
                host_.c_str(), serv_.c_str(), entity_idx_.c_str(),
                ev.host.c_str(), ev.serv.c_str(), ev.entity_idx.c_str());
        return ErrorDef::ParamErr;
    }

    boost::lock_guard<boost::mutex> lock(lock_);
    time_t now = ::time(NULL);
    if (now - ev.time > EventRepos::instance().get_event_linger()) {
        log_err("Too old report: %s %ld - %ld, drop it!", identity_.c_str(), now, ev.time);
        return ErrorDef::TimeoutErr;
    }

    return do_add_event(ev.time, ev.data);
}

int EventHandler::do_add_event(time_t ev_time, const std::vector<event_data_t>& data) {

    auto timed_iter = events_.find(ev_time);
    if (timed_iter == events_.end()) {
        log_debug("create new time slot: %ld", ev_time);
        events_[ev_time] = std::make_shared<events_t>(ev_time);
        timed_iter = events_.find(ev_time);
    }
    SAFE_ASSERT(timed_iter != events_.end());
    std::map<std::string, std::vector<event_data_t>>& timed_slot = timed_iter->second->data;

    for (auto iter = data.begin(); iter != data.end(); ++iter) {
        std::map<std::string, std::vector<event_data_t>>::iterator event_iter = timed_slot.find(iter->name);
        if (event_iter == timed_slot.end()) {
            log_debug("create new event slot: %s in %s@%ld", iter->name.c_str(), identity_.c_str(), ev_time);
            timed_slot[iter->name] = std::vector<event_data_t>();
            event_iter = timed_slot.find(iter->name);
        }

        event_iter->second.push_back(*iter);
    }
    return ErrorDef::OK;
}


struct stat_info_t {
    int count;
    int64_t value_sum;
};

void calc_event_data(const event_data_t& data, std::map<std::string, stat_info_t>& info) {
    if (info.find(data.flag) == info.end()) {
        info[data.flag] = stat_info_t {};
    }
    info[data.flag].count += 1;
    info[data.flag].value_sum += data.value;
}

// process thread
//
// 虽然在handler中直接操作数据库也是可以的，但是一次数据库几十、上百毫秒的延迟会极大的
// 拖慢系统的性能，同时许多进程访问数据库也会导致高并发数据库访问下的各种问题
//

void EventHandler::run() {

    log_info("EventHandler thread begin to run ...");

    EventSql::ev_stat_t stat {};
    stat.host = host_;
    stat.serv = serv_;
    stat.entity_idx = entity_idx_;

    while (true) {

        events_ptr_t event;
        if( !process_queue_.POP(event, 5000) ){
            continue;
        }

        stat.host = host_;
        stat.serv = serv_;
        stat.entity_idx = entity_idx_;
        stat.time = event->time;
        auto& event_slot = event->data;

        // process event
        for (auto iter = event_slot.begin(); iter != event_slot.end(); ++iter) {
            auto& events_name = iter->first;
            auto& events_info = iter->second;
            log_debug("process event %s, count %d", events_name.c_str(), static_cast<int>(events_info.size()));

            std::map<std::string, stat_info_t> flag_info;

            std::for_each(events_info.begin(), events_info.end(),
                          boost::bind(calc_event_data, _1, std::ref(flag_info)));

            // log_debug("process reuslt for event: %s", events_name.c_str());
            for (auto it = flag_info.begin(); it != flag_info.end(); ++it) {
                log_debug("flag %s, count %d, value %ld", it->first.c_str(),
                          static_cast<int>(flag_info[it->first].count), flag_info[it->first].value_sum);


                SAFE_ASSERT(flag_info[it->first].count != 0);

                stat.name = events_name;
                stat.flag = iter->first;
                stat.count = flag_info[it->first].count;
                stat.value_sum = flag_info[it->first].value_sum;
                stat.value_avg = flag_info[it->first].value_sum / flag_info[it->first].count;

                if (insert_ev_stat(stat) != ErrorDef::OK) {
                    log_err("store for %s-%ld name:%s, flag:%s failed!", identity_.c_str(), stat.time,
                            stat.name.c_str(), stat.flag.c_str());
                } else {
                    log_debug("store for %s-%ld name:%s, flag:%s ok!", identity_.c_str(), stat.time,
                            stat.name.c_str(), stat.flag.c_str());
                }
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

    if (!get_config_value("mysql.database", EventSql::database ) ||
        !get_config_value("mysql.table_prefix", EventSql::table_prefix)) {
        return false;
    }
    log_info("We will use database %s, with table_prefix %s.", EventSql::database.c_str(), EventSql::table_prefix.c_str());

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

    std::shared_ptr<EventHandler> handler;
    if (find_or_create_event_handler(evs, handler) != 0) {
        return ErrorDef::Error;
    }

    return handler->add_event(evs);
}
