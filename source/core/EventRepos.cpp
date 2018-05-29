#include <functional>

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

    check_timer_id_ = helper::register_timer_task(std::bind(&EventHandler::check_timer_run, shared_from_this()), 500, true, true);
    if (check_timer_id_ == 0) {
        log_err("Register check_timer failed! ");
        return false;
    }

    thread_ptr_.reset(new std::thread(std::bind(&EventHandler::run, shared_from_this())));
    if (!thread_ptr_){
        log_err("create work thread failed! ");
        return false;
    }

    if (!get_config_value("core.max_process_queue_size", max_process_queue_size_) ||
        max_process_queue_size_ <= 0 )
    {
        LOG("find core.max_process_queue_size failed, set default to 5");
        max_process_queue_size_ = 5;
    }


    return true;
}


EventHandler::~EventHandler() {

    helper::revoke_timer_task(check_timer_id_);
    check_timer_id_ = -1;

    log_err("EventHandler %s destroied...", identity_.c_str());
}

void EventHandler::check_timer_run() {

    std::lock_guard<std::mutex> lock(lock_);

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

    std::lock_guard<std::mutex> lock(lock_);
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
    int64_t value_avg;
    double  value_std;
    std::vector<int64_t> values;
};

static void calc_event_stage1(const event_data_t& data, std::map<std::string, stat_info_t>& infos) {
    if (infos.find(data.flag) == infos.end()) {
        infos[data.flag] = stat_info_t {};
    }
    infos[data.flag].values.push_back(data.value);
    infos[data.flag].count += 1;
    infos[data.flag].value_sum += data.value;
}

static void calc_event_info(std::vector<event_data_t>& data,
                            std::map<std::string, stat_info_t>& infos) {

    // 消息检查和排重
    std::set<int64_t> ids;
    for (auto iter = data.begin(); iter != data.end(); ++iter) {
        ids.insert(iter->msgid);
    }
    if (ids.size() != data.size()) {
        log_err("mismatch size, may contain duplicate items: %d - %d",
                static_cast<int>(ids.size()), static_cast<int>(data.size()));

        size_t expected_size = ids.size();
        // 进行去重复
        std::vector<event_data_t> new_data;
        for (auto iter = data.begin(); iter != data.end(); ++iter) {
            if (ids.find(iter->msgid) != ids.end()) {
                new_data.push_back(*iter);
                ids.erase(iter->msgid);
            }
        }

        SAFE_ASSERT(new_data.size() == expected_size);
        data = std::move(new_data);
    }


    std::for_each(data.begin(), data.end(),
                  std::bind(calc_event_stage1, std::placeholders::_1, std::ref(infos)));

    // calc avg and std
    for (auto iter = infos.begin(); iter!= infos.end(); ++iter) {
        auto& info = iter->second;
        info.value_avg = info.value_sum / info.count;

        double sum = 0;
        for (size_t i=0; i< info.values.size(); ++i) {
            sum +=  ::pow(info.values[i] - info.value_avg, 2);
        }
        info.value_std = ::sqrt(sum / info.values.size());
    }
}

static int stateless_process_event(events_ptr_t event, event_insert_t copy_stat) {

    auto& event_slot = event->data;

    // process event
    for (auto iter = event_slot.begin(); iter != event_slot.end(); ++iter) {
        auto& events_name = iter->first;
        auto& events_info = iter->second;
        log_debug("process event %s, count %d", events_name.c_str(), static_cast<int>(events_info.size()));

        std::map<std::string, stat_info_t> flag_info;
        calc_event_info(events_info, flag_info);

        // log_debug("process reuslt for event: %s", events_name.c_str());
        for (auto it = flag_info.begin(); it != flag_info.end(); ++it) {
            log_debug("flag %s, count %d, value %ld", it->first.c_str(),
                      static_cast<int>(flag_info[it->first].count), flag_info[it->first].value_sum);


            SAFE_ASSERT(flag_info[it->first].count != 0);

            copy_stat.name = events_name;
            copy_stat.flag = it->first;
            copy_stat.count = flag_info[it->first].count;
            copy_stat.value_sum = flag_info[it->first].value_sum;
            copy_stat.value_avg = flag_info[it->first].value_avg;
            copy_stat.value_std = flag_info[it->first].value_std;

            if (EventSql::insert_ev_stat(copy_stat) != ErrorDef::OK) {
                log_err("store for (%s, %s, %s) - %ld name:%s, flag:%s failed!",
                        copy_stat.host.c_str(), copy_stat.serv.c_str(), copy_stat.entity_idx.c_str(),
                        copy_stat.time, copy_stat.name.c_str(), copy_stat.flag.c_str());
            } else {
                log_debug("store for (%s, %s, %s) - %ld name:%s, flag:%s ok!",
                        copy_stat.host.c_str(), copy_stat.serv.c_str(), copy_stat.entity_idx.c_str(),
                        copy_stat.time, copy_stat.name.c_str(), copy_stat.flag.c_str());
            }
        }
    }

    return ErrorDef::OK;
}



void EventHandler::run_once_task(std::vector<events_ptr_t> events) {

    LOG("TzMonitorEventHandler run_once_task thread %#lx begin to run ...", (long)pthread_self());

    for(auto iter = events.begin(); iter != events.end(); ++iter) {

        event_insert_t stat {};
        stat.host = host_;
        stat.serv = serv_;
        stat.entity_idx = entity_idx_;
        stat.time = (*iter)->time;

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
    stat.host = host_;
    stat.serv = serv_;
    stat.entity_idx = entity_idx_;

    while (true) {

        while (process_queue_.SIZE() > 2 * max_process_queue_size_) {
            std::vector<events_ptr_t> ev_inserts;
            size_t ret = process_queue_.POP(ev_inserts, max_process_queue_size_, 5000);
            if (!ret) {
                break;
            }

            std::function<void()> func = std::bind(&EventHandler::run_once_task, shared_from_this(), ev_inserts);
            EventRepos::instance().add_task(func);
        }

        events_ptr_t event;
        if( !process_queue_.POP(event, 5000) ){
            continue;
        }

        stat.host = host_;
        stat.serv = serv_;
        stat.entity_idx = entity_idx_;
        stat.time = event->time;

        // do actual handle
        stateless_process_event(event, stat);
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

    if (!get_config_value("core.max_process_task_size", max_process_task_size_) ||
        max_process_task_size_ <= 0 )
    {
         LOG("find core.max_process_task_size failed, set default to 10");
         max_process_task_size_ = 10;
    }
    task_helper_ = std::make_shared<TinyTask>(max_process_task_size_);
    if (!task_helper_ || !task_helper_->init()){
        LOG("create task_helper work thread failed! ");
        return false;
    }

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


int EventRepos::get_event(const event_cond_t& cond, event_query_t& stat) {
    if (cond.version != "1.0.0" || cond.interval_sec <=0 || cond.name.empty()) {
        log_err("param check error: %s, %ld, %s", cond.version.c_str(), cond.interval_sec, cond.name.c_str());
        return ErrorDef::ParamErr;
    }
    return EventSql::query_ev_stat(cond, stat);
}

