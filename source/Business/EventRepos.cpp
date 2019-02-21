/*-
 * Copyright (c) 2018 TAO Zhijiang<taozhijiang@gmail.com>
 *
 * Licensed under the BSD-3-Clause license, see LICENSE for full information.
 *
 */


#include <functional>

#include <Scaffold/ConfHelper.h>
#include <Utils/Utils.h>
#include <Utils/StrUtil.h>

#include <Business/EventHandler.h>
#include <Business/StoreSql.h>


#include <Business/EventDispatcherh>

using namespace tzrpc;

EventRepos& EventRepos::instance() {
    static EventRepos handler {};
    return handler;
}


bool EventRepos::init() {

    auto conf_ptr = ConfHelper::instance().get_conf();
    if (!conf_ptr) {
        return false;
    }

    const auto& conf = *conf_ptr;
    ConfUtil::conf_value(conf, "business.support_task_size", support_task_size_);
    if (value_i <= 0) {
        log_err("Invalid business.support_task_size: %l ", value_i);
        return false;
    }
    support_task_size_ = value_i;


    ConfUtil::conf_value(conf, "business.event_linger", value_i);
    if (value_i <= 0) {
        log_err("Invalid business.event_linger: %d ", value_i);
        return false;
    }
    conf_.event_linger_ = value_i;

    ConfUtil::conf_value(conf, "business.event_step", value_i);
    if (value_i <= 0) {
        log_err("Invalid business.event_step: %d ", value_i);
        return false;
    }
    conf_.event_step_ = value_i;


    ConfUtil::conf_value(conf, "business.process_queue_size", value_i);
    if (value_i <= 0) {
        log_err("Invalid business.process_queue_size: %l ", value_i);
        return false;
    }
    conf_.process_queue_size_ = value_i;



    log_debug("EventRepos Conf info \n"
              "event_linger %d, event_step %d, process_queue_size %d, support_task_size %d",
              conf_.event_linger_.load(), conf_.event_step_.load(),
              conf_.process_queue_size_.load(), conf_.support_task_size_.load());


    store_.reset(new StoreSql());
    if (!store_ || !store_->init(conf)) {
        log_err("Init StoreSql failed.");
        return false;
    }

    return true;
}


int EventRepos::destory_handlers() {
    handlers_.clear();
    return 0;
}


// forward request to specified handlers
int EventRepos::add_event(const event_report_t& evs) {

    if (evs.service.empty() || evs.entity_idx.empty() || evs.data.empty()) {
        log_err("evs param check error!");
        return -1;
    }

    std::shared_ptr<EventHandler> handler;
    if (find_or_create_event_handler(evs, handler) != 0) {
        return -1;
    }

    return handler->add_event(evs);
}


int EventRepos::get_event(const event_cond_t& cond, event_select_t& stat) {
    if (cond.version != "1.0.0" ||
        cond.tm_interval <=0 || cond.metric.empty())
    {
        log_err("param check error: %s, %ld, %s",
                cond.version.c_str(), cond.tm_interval, cond.metric.c_str());
        return -1;
    }

    return store_->select_ev_stat(cond, stat);
}


// 只会增加handler
// TODO 使用智能指针进行读写数据的优化
int EventRepos::find_or_create_event_handler(const event_report_t& evs, std::shared_ptr<EventHandler>& handler) {

    if (evs.service.empty() || evs.entity_idx.empty()) {
        log_err("required identity info empty!");
        return -1;
    }

    std::string identity = construct_identity(evs.service, evs.entity_idx);
    std::lock_guard<std::mutex> lock(lock_);

    auto iter = handlers_.find(identity);
    if (iter != handlers_.end()) {
        handler = iter->second;
        return 0;
    }

    // create new handler
    std::shared_ptr<EventHandler> new_handler;
    if (do_create_event_handler(evs, new_handler) == 0) {
        handler = new_handler;
        return 0;
    }

    log_err("create handler with %s failed.", identity.c_str());
    return -1;
}

// should be call with lock already hold
int EventRepos::do_create_event_handler(const event_report_t& evs, std::shared_ptr<EventHandler>& new_handler) {

    std::string identity = construct_identity(evs.service, evs.entity_idx);
    std::shared_ptr<EventHandler> handler = std::make_shared<EventHandler>(evs.service, evs.entity_idx);
    if (!handler || !handler->init()) {
        log_err("Create handler %s failed!", identity.c_str());
        return -1;
    }

    handlers_[identity] = handler;
    new_handler = handler;
    return 0;
}
