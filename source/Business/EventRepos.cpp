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


#include <Business/EventRepos.h>

using namespace tzrpc;

EventRepos& EventRepos::instance() {
    static EventRepos handler {};
    return handler;
}


bool EventRepos::init() {

    handlers_.reset(new HandlerType());
    if (!handlers_) {
        log_err("create HandlerType failed.");
        return false;
    }

    auto conf_ptr = ConfHelper::instance().get_conf();
    if (!conf_ptr) {
        return false;
    }

    ConfUtil::conf_value(*conf_ptr, "rpc_business.support_process_task_size", support_process_task_size_);
    if (support_process_task_size_ <= 0) {
        log_err("Invalid business.support_task_size: %d ", support_process_task_size_);
        return false;
    }

    support_task_helper_ = std::make_shared<tzrpc::TinyTask>(support_process_task_size_);
    if (!support_task_helper_ || !support_task_helper_->init()){
        log_err("create task_helper work thread failed! ");
        return false;
    }

    try {

        // initialize event handler default conf
        const libconfig::Setting& rpc_handlers = conf_ptr->lookup("rpc_business.services");

        // 遍历，找出默认配置信息
        for(int i = 0; i < rpc_handlers.getLength(); ++i) {

            const libconfig::Setting& handler_conf = rpc_handlers[i];
            std::string instance_name;

            ConfUtil::conf_value(handler_conf, "service_name", instance_name);
            if (instance_name == "[default]") {

                // 创建对象
                default_handler_conf_.reset(new EventHandlerConf());
                if (!default_handler_conf_) {
                    log_err("create EventHandlerConf failed.");
                    return false;
                }

                int value_i;
                ConfUtil::conf_value(handler_conf, "event_linger", value_i);
                if (value_i <= 0) {
                    log_err("Invalid event_linger: %d ", value_i);
                    return false;
                }
                default_handler_conf_->event_linger_ = value_i;

                ConfUtil::conf_value(handler_conf, "event_step", value_i);
                if (value_i <= 0) {
                    log_err("Invalid event_step: %d ", value_i);
                    return false;
                }
                default_handler_conf_->event_step_ = value_i;


                ConfUtil::conf_value(handler_conf, "additional_process_queue_size", value_i);
                if (value_i <= 0) {
                    log_err("Invalid additional_process_queue_size: %l ", value_i);
                    return false;
                }
                default_handler_conf_->additional_process_queue_size_ = value_i;

                std::string store_type;
                ConfUtil::conf_value(handler_conf, "store_type", store_type);
                if (store_type != "mysql" && store_type != "redis" && store_type != "leveldb") {
                    log_err("Invalid store_type: %s ", store_type.c_str());
                    return false;
                }
                default_handler_conf_->store_type_ = store_type;

                log_debug("EventHandlerConf default template info \n"
                          "event_linger %d, event_step %d, process_queue_size %d, store_type %s",
                          default_handler_conf_->event_linger_.load(),
                          default_handler_conf_->event_step_.load(),
                          default_handler_conf_->additional_process_queue_size_.load(),
                          default_handler_conf_->store_type_.c_str());

                break;
            }
        }


    } catch (...) {
        log_err("find setting of rpc_business.services failed.");
        return false;
    }


    if (!default_handler_conf_) {
        log_err("initialize event_handler default conf failed.");
        return false;
    }

    // test default store
    auto default_store = StoreFactory(default_handler_conf_->store_type_);
    if (!default_store) {
        log_err("default store implement %s not OK!", default_handler_conf_->store_type_.c_str());
        return false;
    }

    return true;
}


int EventRepos::destory_handlers() {
    handlers_.reset();
    return 0;
}


// forward request to specified handlers
int EventRepos::add_event(const event_report_t& evs) {

    if (evs.version != "1.0.0" ||
        evs.timestamp <= 0 ||
        evs.service.empty() || evs.entity_idx.empty() || evs.data.empty()) {
        log_err("add_event param check failed!");
        return -1;
    }

    std::shared_ptr<EventHandler> handler;
    if (find_create_event_handler(evs.service, evs.entity_idx, handler) != 0) {
        log_err("find_or_create_event_handler for %s,%s failed.",
                evs.service.c_str(), evs.entity_idx.c_str());
        return -1;
    }

    SAFE_ASSERT(handler);
    return handler->add_event(evs);
}


int EventRepos::get_event(const event_cond_t& cond, event_select_t& stat) {

    if (cond.version != "1.0.0" ||
        cond.service.empty() || cond.tm_interval < 0 || cond.metric.empty()) {
        log_err("get_event param check failed!");
        return -1;
    }

    std::shared_ptr<EventHandler> handler;
    if (find_create_event_handler(cond.service, cond.entity_idx, handler) != 0) {
        log_err("find_or_create_event_handler for %s,%s failed.",
                cond.service.c_str(), cond.entity_idx.c_str());
        return -1;
    }

    SAFE_ASSERT(handler);
    return handler->get_event(cond, stat);
}


// 只会增加handler
// TODO 使用智能指针进行读写数据的优化
int EventRepos::find_create_event_handler(const std::string& service, const std::string& entity_idx,
                                          std::shared_ptr<EventHandler>& handler) {

    std::string identity = construct_identity(service, entity_idx);
    std::shared_ptr<HandlerType> handlers;
    {
        std::unique_lock<std::mutex> lock(lock_);
        handlers = handlers_;
    }

    SAFE_ASSERT(handlers);

    auto iter = handlers->find(identity);
    if (iter != handlers->end()) {
        handler = iter->second;
        return 0;
    }

    // 持锁来处理
    {
        // double check
        std::unique_lock<std::mutex> lock(lock_);
        auto iter = handlers_->find(identity);
        if (iter != handlers_->end()) {
            handler = iter->second;
            return 0;
        }

        std::shared_ptr<EventHandler> new_handler = std::make_shared<EventHandler>(service, entity_idx);
        if (!new_handler || !new_handler->init()) {
            log_err("create handler %s:%s failed!",
                    service.c_str(), entity_idx.c_str());
            return -1;
        }

        (*handlers_)[identity] = new_handler;
        handler = new_handler;

        return 0;
    }
}



EventHandlerConf EventRepos::get_default_handler_conf() {
    return *default_handler_conf_;
}


