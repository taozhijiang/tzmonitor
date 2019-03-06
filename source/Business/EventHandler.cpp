/*-
 * Copyright (c) 2019 TAO Zhijiang<taozhijiang@gmail.com>
 *
 * Licensed under the BSD-3-Clause license, see LICENSE for full information.
 *
 */

#include <algorithm>

#include <xtra_rhel.h>
#include <functional>

#include <Utils/Log.h>
#include <Utils/Timer.h>


#include <Business/EventHandler.h>
#include <Business/EventRepos.h>

using namespace tzrpc;

// EventHandler

bool EventHandler::init() {

    if (service_.empty()) {
        log_err("service not initialized %s", service_.c_str());
        return false;
    }

    // 首先加载默认配置
    conf_ = EventRepos::instance().get_default_handler_conf();

    // 检测是否有覆盖配置
    auto conf_ptr = ConfHelper::instance().get_conf();
    if (!conf_ptr) {
        log_err("ConfHelper not initialized, please check your initialize order.");
        return false;
    }

    try {

        const libconfig::Setting& rpc_handlers = conf_ptr->lookup("rpc.business.services");

        for(int i = 0; i < rpc_handlers.getLength(); ++i) {

            const libconfig::Setting& handler_conf = rpc_handlers[i];
            std::string instance_name;
            handler_conf.lookupValue("instance_name", instance_name);

            if (instance_name == service_) {

                int value_i;
                handler_conf.lookupValue("event_linger", value_i);
                if (value_i > 0) {
                    conf_.event_linger_ = value_i;
                }

                handler_conf.lookupValue("event_step", value_i);
                if (value_i > 0) {
                    conf_.event_step_ = value_i;
                }

                handler_conf.lookupValue("additional_process_step_size", value_i);
                if (value_i > 0) {
                    conf_.additional_process_step_size_ = value_i;
                }

                std::string store_type;
                handler_conf.lookupValue("store_type", store_type);
                if (store_type == "mysql" || store_type == "redis" || store_type == "leveldb") {
                    conf_.store_type_ = store_type;
                }

                break;

            }
        }

        log_debug("EventHandlerConf for %s final info \n"
                  "event_linger %d, event_step %d, process_step_size %d, store_type %s",
                  service_.c_str(),
                  conf_.event_linger_,
                  conf_.event_step_,
                  conf_.additional_process_step_size_,
                  conf_.store_type_.c_str());

    } catch (const libconfig::SettingNotFoundException &nfex) {
        log_err("rpc.business.services not found!");
    } catch (std::exception& e) {
        log_err("execptions catched for %s",  e.what());
    }

    store_ = StoreFactory(conf_.store_type_);
    if (!store_) {
        log_err("store implement %s for %s not OK!",
                conf_.store_type_.c_str(), service_.c_str());
        return false;
    }

    thread_ptr_.reset(new boost::thread(std::bind(&EventHandler::run, shared_from_this())));
    if (!thread_ptr_){
        log_err("create work thread failed! ");
        return false;
    }

    if (!Timer::instance().add_timer(std::bind(&EventHandler::linger_check_run, shared_from_this()), 500, true)) {
        log_err("add linger_check_run failed.");
        return false;
    }

    return true;
}


int EventHandler::update_runtime_conf(const libconfig::Config& conf) {

    try {

        // initialize event handler default conf
        const libconfig::Setting& rpc_handlers = conf.lookup("rpc.business.services");

        // 遍历，找出默认配置信息
        for(int i = 0; i < rpc_handlers.getLength(); ++i) {

            const libconfig::Setting& handler_conf = rpc_handlers[i];
            std::string instance_name;

            handler_conf.lookupValue("service_name", instance_name);
            if (instance_name == service_) {

                log_notice("find specific conf for service %s", service_.c_str());

                int value_i;
                if (handler_conf.lookupValue("event_linger", value_i) && value_i > 0) {
                    log_notice("update default event_linger from %d to %d",
                               conf_.event_linger_, value_i );
                    conf_.event_linger_ = value_i;
                }

                if (handler_conf.lookupValue("event_step", value_i) && value_i > 0) {
                    log_notice("update default event_step from %d to %d",
                               conf_.event_step_, value_i );
                    conf_.event_step_ = value_i;
                }

                if (handler_conf.lookupValue("additional_process_step_size", value_i) && value_i > 0) {
                    log_notice("update default additional_process_step_size from %d to %d",
                               conf_.additional_process_step_size_, value_i );
                    conf_.additional_process_step_size_ = value_i;
                }

                log_debug("EventHandlerConf for service %s template info \n"
                          "event_linger %d, event_step %d, process_step_size %d, store_type %s",
                          service_.c_str(),
                          conf_.event_linger_,
                          conf_.event_step_,
                          conf_.additional_process_step_size_,
                          conf_.store_type_.c_str());

                break;
            }
        }


    } catch (const libconfig::SettingNotFoundException &nfex) {
        log_err("rpc.business.services not found!");
    } catch (std::exception& e) {
        log_err("execptions catched for %s",  e.what());
    }

    return 0;
}

int EventHandler::module_status(std::string& strModule, std::string& strKey, std::string& strValue) {

    // empty

    return 0;
}



int EventHandler::add_event(const event_report_t& ev) {

    if (ev.service != service_) {
        log_err("error for service check, expect %s != %s",
                ev.service.c_str(), service_.c_str());
        return -1;
    }

    time_t now = ::time(NULL);
    if (now - ev.timestamp > conf_.event_linger_) {
        log_err("critical... too old report: %s %ld - %ld = %ld, drop it!",
                service_.c_str(), now, ev.timestamp, (now - ev.timestamp));
        return -1;
    }

    std::lock_guard<std::mutex> lock(lock_);
    return do_add_event(ev.timestamp, ev.data);
}


int EventHandler::do_add_event(time_t ev_time, const std::vector<event_data_t>& data) {

    // optimize
    ev_time = conf_.nice_step(ev_time);

    auto timed_iter = events_.find(ev_time);
    if (timed_iter == events_.end()) {
        log_debug("create new time slot: %ld", ev_time);
        events_[ev_time] = std::make_shared<events_by_time_t>(ev_time, conf_.event_step_);
        timed_iter = events_.find(ev_time);
    }

    SAFE_ASSERT(timed_iter != events_.end());

    // key: metric
    auto& timed_slot = timed_iter->second->data_;

    for (auto iter = data.begin(); iter != data.end(); ++iter) {
        auto metric_iter = timed_slot.find(iter->metric);
        if (metric_iter == timed_slot.end()) {
            log_debug("create new metric %s at time_slot: %ld in %s",
                      iter->metric.c_str(), ev_time, service_.c_str(), ev_time);

            timed_slot[iter->metric] = std::vector<event_data_t>();
            metric_iter = timed_slot.find(iter->metric);
        }

        metric_iter->second.push_back(*iter);
    }

    return 0;
}

void EventHandler::linger_check_run() {

    std::lock_guard<std::mutex> lock(lock_);
    time_t now = ::time(NULL);

    for (auto iter = events_.begin(); iter != events_.end(); /*nop*/) {
        if (iter->first + conf_.event_linger_ < now) {

            process_queue_.PUSH(iter->second);

            // References and iterators to the erased elements are invalidated.
            // Other references and iterators are not affected.
            events_.erase(iter++);

        } else {
            break;
        }
    }
}


int EventHandler::get_event(const event_cond_t& cond, event_select_t& stat) {

    if (!store_) {
        log_err("store not initialized with storetype: %s", conf_.store_type_.c_str());
        return -1;
    }

    return store_->select_ev_stat(cond, stat, conf_.event_linger_);
}

void EventHandler::run_once_task(std::vector<events_by_time_ptr_t> events) {

    log_debug("MonitorEventHandler run_once_task thread %#lx begin to run ...", (long)pthread_self());

    for(auto iter = events.begin(); iter != events.end(); ++iter) {

        event_insert_t stat {};
        stat.service = service_;
        stat.entity_idx = entity_idx_;
        stat.timestamp = (*iter)->timestamp_;
        stat.step = (*iter)->step_;

        do_process_event(*iter, stat);
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

        int queue_size = conf_.additional_process_step_size_;

        // 如果积累的待处理任务比较多，就取出来给辅助线程处理
        while (process_queue_.SIZE() > 2 * queue_size) {
            std::vector<events_by_time_ptr_t> ev_inserts;
            size_t ret = process_queue_.POP(ev_inserts, queue_size, 10);
            if (!ret) {
                log_err("Pop support task failed.");
                break;
            }

            std::function<void()> func = std::bind(&EventHandler::run_once_task, shared_from_this(), ev_inserts);
            EventRepos::instance().add_additional_task(func);
        }

        events_by_time_ptr_t event;
        if( !process_queue_.POP(event, 1000) ){
            continue;
        }

        stat.timestamp = event->timestamp_;
        stat.step = event->step_;

        // do actual handle
        do_process_event(event, stat);
    }
}



// 数据处理部分


// 内部使用临时结构体
struct stat_info_t {
    int32_t count;
    int64_t value_sum;
    int32_t value_avg;
    int32_t value_min;
    int32_t value_max;
    int32_t value_p10;
    int32_t value_p50;
    int32_t value_p90;
    std::vector<int32_t> values;
};

static
void aggregate_by_tag(const event_data_t& data, std::map<std::string, stat_info_t>& infos) {
    if (infos.find(data.tag) == infos.end()) {
        infos[data.tag] = stat_info_t {};
    }
    infos[data.tag].values.push_back(data.value);
    infos[data.tag].count += 1;
    infos[data.tag].value_sum += data.value;
}

static
void calc_event_info_each_metric(std::vector<event_data_t>& data,
                                 std::map<std::string, stat_info_t>& infos) {

    // 消息检查和排重
    std::set<int64_t> ids;
    for (auto iter = data.begin(); iter != data.end(); ++iter) {
        ids.insert(iter->msgid);
    }

    if (ids.size() != data.size()) {
        log_err("mismatch size, may contain duplicate items: %d - %d",
                static_cast<int>(ids.size()), static_cast<int>(data.size()));

        // dump出重复的事件
        for(size_t i=0; i<data.size(); ++i) {
            for(size_t j=i+1; j<data.size(); ++j) {
                if (data[i].msgid == data[j].msgid) {
                    log_err("found dumpicate message: %lu - %lu, (%ld %ld %s), (%ld %ld %s)", i, j,
                            data[i].msgid, data[i].value, data[i].tag.c_str(),
                            data[j].msgid, data[j].value, data[j].tag.c_str());
                }
            }
        }

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
                  std::bind(aggregate_by_tag, std::placeholders::_1, std::ref(infos)));


    // calc avg and std
    for (auto iter = infos.begin(); iter!= infos.end(); ++iter) {

        auto& info = iter->second;
        if (info.count == 0) {
            log_err("info.count == 0 ???");
            continue;
        }

        info.value_avg = info.value_sum / info.count;

        // 采用 nth_element算法
        // 计算 p50, p90
        size_t len = info.values.size();

        std::nth_element(info.values.begin(), info.values.begin(), info.values.end());
        info.value_min = info.values[0];
        std::nth_element(info.values.begin(), info.values.begin(), info.values.end(), std::greater<int32_t>());
        info.value_max = info.values[0];

        size_t p10_idx = len * 0.1;
        std::nth_element(info.values.begin(), info.values.begin() + p10_idx, info.values.end());
        info.value_p10 = info.values[p10_idx];

        size_t p50_idx = len * 0.5;
        std::nth_element(info.values.begin(), info.values.begin() + p50_idx, info.values.end());
        info.value_p50 = info.values[p50_idx];

        size_t p90_idx = len * 0.9;
        std::nth_element(info.values.begin(), info.values.begin() + p90_idx, info.values.end());
        info.value_p90 = info.values[p90_idx];
    }
}

// 无状态的处理函数
int EventHandler::do_process_event(events_by_time_ptr_t event, event_insert_t copy_stat) {

    auto& event_slot = event->data_;

    // process event
    for (auto iter = event_slot.begin(); iter != event_slot.end(); ++iter) {
        auto& events_metric = iter->first;
        auto& events_info = iter->second;
        log_debug("process event %s, count %d", events_metric.c_str(), static_cast<int>(events_info.size()));

        std::map<std::string, stat_info_t> tag_info;
        calc_event_info_each_metric(events_info, tag_info);

        // log_debug("process reuslt for event: %s", events_name.c_str());
        for (auto it = tag_info.begin(); it != tag_info.end(); ++it) {
            log_debug("tag %s, count %d, value %ld",
                      it->first.c_str(),
                      static_cast<int>(tag_info[it->first].count), tag_info[it->first].value_sum);

            SAFE_ASSERT(tag_info[it->first].count != 0);

            copy_stat.metric = events_metric;
            copy_stat.tag = it->first;
            copy_stat.count = tag_info[it->first].count;
            copy_stat.value_sum = tag_info[it->first].value_sum;
            copy_stat.value_avg = tag_info[it->first].value_avg;
            copy_stat.value_min = tag_info[it->first].value_min;
            copy_stat.value_max = tag_info[it->first].value_max;
            copy_stat.value_p10 = tag_info[it->first].value_p10;
            copy_stat.value_p50 = tag_info[it->first].value_p50;
            copy_stat.value_p90 = tag_info[it->first].value_p90;

            if (!store_ || store_->insert_ev_stat(copy_stat) != 0) {
                log_err("store for (%s, %s) - %ld name:%s, flag:%s failed!",
                        copy_stat.service.c_str(), copy_stat.entity_idx.c_str(),
                        copy_stat.timestamp, copy_stat.metric.c_str(), copy_stat.tag.c_str());
            } else {
                log_debug("store for (%s, %s) - %ld name:%s, flag:%s ok!",
                          copy_stat.service.c_str(), copy_stat.entity_idx.c_str(),
                          copy_stat.timestamp, copy_stat.metric.c_str(), copy_stat.tag.c_str());
            }
        }
    }

    return 0;

}
