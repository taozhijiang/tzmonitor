/*-
 * Copyright (c) 2019 TAO Zhijiang<taozhijiang@gmail.com>
 *
 * Licensed under the BSD-3-Clause license, see LICENSE for full information.
 *
 */


#include <functional>

#include <Business/EventHandler.h>
#include <Business/StoreIf.h>
#include <Business/EventRepos.h>

#include <Captain.h>

EventRepos& EventRepos::instance() {
    static EventRepos handler{};
    return handler;
}


bool EventRepos::init() {

    handlers_.reset(new HandlerType());
    if (!handlers_) {
        roo::log_err("create HandlerType failed.");
        return false;
    }

    auto setting_ptr = Captain::instance().setting_ptr_->get_setting();
    if (!setting_ptr) {
        roo::log_err("Setting not initialized? return setting_ptr empty!!!");
        return false;
    }

    setting_ptr->lookupValue("rpc.business.support_process_task_size", support_process_task_size_);
    if (support_process_task_size_ <= 0) {
        roo::log_err("Invalid business.support_task_size: %d ", support_process_task_size_);
        return false;
    }

    support_task_helper_ = std::make_shared<roo::ThreadMng>(support_process_task_size_);
    if (!support_task_helper_) {
        roo::log_err("create task_helper work thread failed! ");
        return false;
    }

    try {

        // initialize event handler default conf
        const libconfig::Setting& rpc_handlers = setting_ptr->lookup("rpc.business.services");

        // 遍历，找出默认配置信息
        for (int i = 0; i < rpc_handlers.getLength(); ++i) {

            const libconfig::Setting& handler_conf = rpc_handlers[i];
            std::string instance_name;

            handler_conf.lookupValue("service_name", instance_name);
            if (instance_name == "[default]") {

                // 创建对象
                default_handler_conf_.reset(new EventHandlerConf());
                if (!default_handler_conf_) {
                    roo::log_err("create EventHandlerConf failed.");
                    return false;
                }

                int value_i;
                handler_conf.lookupValue("event_linger", value_i);
                if (value_i <= 0) {
                    roo::log_err("Invalid event_linger: %d ", value_i);
                    return false;
                }
                default_handler_conf_->event_linger_ = value_i;

                handler_conf.lookupValue("event_step", value_i);
                if (value_i <= 0) {
                    roo::log_err("Invalid event_step: %d ", value_i);
                    return false;
                }
                default_handler_conf_->event_step_ = value_i;


                handler_conf.lookupValue("additional_process_step_size", value_i);
                if (value_i <= 0) {
                    roo::log_err("Invalid additional_process_step_size: %d ", value_i);
                    return false;
                }
                default_handler_conf_->additional_process_step_size_ = value_i;

                std::string store_type;
                handler_conf.lookupValue("store_type", store_type);
                if (store_type != "mysql" && store_type != "redis" && store_type != "leveldb") {
                    roo::log_err("Invalid store_type: %s ", store_type.c_str());
                    return false;
                }
                default_handler_conf_->store_type_ = store_type;

                roo::log_info("EventHandlerConf default template info \n"
                              "event_linger %d, event_step %d, process_step_size %d, store_type %s",
                              default_handler_conf_->event_linger_,
                              default_handler_conf_->event_step_,
                              default_handler_conf_->additional_process_step_size_,
                              default_handler_conf_->store_type_.c_str());

                break;
            }
        }


    } catch (const libconfig::SettingNotFoundException& nfex) {
        roo::log_err("rpc.business.services not found!");
    } catch (std::exception& e) {
        roo::log_err("execptions catched for %s",  e.what());
    }


    if (!default_handler_conf_) {
        roo::log_err("initialize event_handler default conf failed.");
        return false;
    }

    // test default store
    auto default_store = StoreFactory(default_handler_conf_->store_type_);
    if (!default_store) {
        roo::log_err("default store implement %s not OK!", default_handler_conf_->store_type_.c_str());
        return false;
    }


    // 注册配置动态更新的回调函数
    Captain::instance().setting_ptr_->attach_runtime_callback(
        "EventRepos",
        std::bind(&EventRepos::module_runtime, this,
                  std::placeholders::_1));

    // 系统状态展示相关的初始化
    Captain::instance().status_ptr_->attach_status_callback(
        "EventRepos",
        std::bind(&EventRepos::module_status, this,
                  std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));


    return true;
}


int EventRepos::destory_handlers() {
    handlers_.reset();
    return 0;
}


// forward request to specified handlers
int EventRepos::add_event(const event_report_t& evs) {

    if (evs.version != "1.0.0" ||
        evs.timestamp <= 0 || evs.service.empty() || evs.data.empty()) {
        roo::log_err("add_event param check failed!");
        return -1;
    }

    std::shared_ptr<EventHandler> handler;
    if (find_create_event_handler(evs.service, evs.entity_idx, handler) != 0) {
        roo::log_err("find_or_create_event_handler for %s,%s failed.",
                     evs.service.c_str(), evs.entity_idx.c_str());
        return -1;
    }

    SAFE_ASSERT(handler);
    return handler->add_event(evs);
}

// 因为不同的handler可能采用不同的存储实现，所以get操作还是不能直接下放
// 到存储层去执行
int EventRepos::get_event(const event_cond_t& cond, event_select_t& stat) {

    if (cond.version != "1.0.0" ||
        cond.service.empty() || cond.tm_interval < 0 || cond.metric.empty()) {
        roo::log_err("get_event param check failed!");
        return -1;
    }

    std::shared_ptr<EventHandler> handler;
    if (find_create_event_handler(cond.service, cond.entity_idx, handler) != 0) {
        roo::log_err("find_or_create_event_handler for %s,%s failed.",
                     cond.service.c_str(), cond.entity_idx.c_str());
        return -1;
    }

    SAFE_ASSERT(handler);
    return handler->get_event(cond, stat);
}

int EventRepos::get_metrics(const std::string& version,
                            const std::string& service, std::vector<std::string>& metric_stat) {

    if (version != "1.0.0" || service.empty()) {
        return -1;
    }

    std::set<std::string> unique_store;
    std::vector<std::string> tmp_store;

    auto store = StoreFactory("mysql");
    tmp_store.clear();
    if (!store || store->select_metrics(service, tmp_store) != 0) {
        roo::log_warning("get mysql store failed.");
    } else {
        unique_store.insert(tmp_store.cbegin(), tmp_store.cend());
    }

    store = StoreFactory("redis");
    tmp_store.clear();
    if (!store || store->select_metrics(service, tmp_store) != 0) {
        roo::log_warning("get redis store failed.");
    } else {
        unique_store.insert(tmp_store.cbegin(), tmp_store.cend());
    }

    store = StoreFactory("leveldb");
    tmp_store.clear();
    if (!store || store->select_metrics(service, tmp_store) != 0) {
        roo::log_warning("get leveldb store failed.");
    } else {
        unique_store.insert(tmp_store.cbegin(), tmp_store.cend());
    }

    metric_stat.clear();
    metric_stat.assign(unique_store.cbegin(), unique_store.cend());

    return 0;
}



int EventRepos::get_services(const std::string& version,
                             std::vector<std::string>& service_stat) {

    if (version != "1.0.0") {
        return -1;
    }

    std::set<std::string> unique_store;
    std::vector<std::string> tmp_store;

    auto store = StoreFactory("mysql");
    tmp_store.clear();
    if (!store || store->select_services(tmp_store) != 0) {
        roo::log_warning("get mysql store failed.");
    } else {
        roo::log_info("mysql return service %d item", static_cast<int>(tmp_store.size()));
        unique_store.insert(tmp_store.cbegin(), tmp_store.cend());
    }

    store = StoreFactory("redis");
    tmp_store.clear();
    if (!store || store->select_services(tmp_store) != 0) {
        roo::log_warning("get redis store failed.");
    } else {
        roo::log_info("redis return service %d item", static_cast<int>(tmp_store.size()));
        unique_store.insert(tmp_store.cbegin(), tmp_store.cend());
    }

    store = StoreFactory("leveldb");
    tmp_store.clear();
    if (!store || store->select_services(tmp_store) != 0) {
        roo::log_warning("get leveldb store failed.");
    } else {
        roo::log_info("leveldb return service %d item", static_cast<int>(tmp_store.size()));
        unique_store.insert(tmp_store.cbegin(), tmp_store.cend());
    }

    service_stat.clear();
    service_stat.assign(unique_store.cbegin(), unique_store.cend());

    return 0;
}


int EventRepos::module_runtime(const libconfig::Config& conf) {

    try {

        // initialize event handler default conf
        const libconfig::Setting& rpc_handlers = conf.lookup("rpc.business.services");

        // 遍历，找出默认配置信息
        for (int i = 0; i < rpc_handlers.getLength(); ++i) {

            const libconfig::Setting& handler_conf = rpc_handlers[i];
            std::string instance_name;

            handler_conf.lookupValue("service_name", instance_name);
            if (instance_name == "[default]") {


                int value_i;
                if (handler_conf.lookupValue("event_linger", value_i) && value_i > 0) {
                    roo::log_warning("update default event_linger from %d to %d",
                                     default_handler_conf_->event_linger_, value_i);
                    default_handler_conf_->event_linger_ = value_i;
                }

                if (handler_conf.lookupValue("event_step", value_i) && value_i > 0) {
                    roo::log_warning("update default event_step from %d to %d",
                                     default_handler_conf_->event_step_, value_i);
                    default_handler_conf_->event_step_ = value_i;
                }

                if (handler_conf.lookupValue("additional_process_step_size", value_i) && value_i > 0) {
                    roo::log_warning("update default additional_process_step_size from %d to %d",
                                     default_handler_conf_->additional_process_step_size_, value_i);
                    default_handler_conf_->additional_process_step_size_ = value_i;
                }

                roo::log_info("EventHandlerConf default template info \n"
                              "event_linger %d, event_step %d, process_step_size %d, store_type %s",
                              default_handler_conf_->event_linger_,
                              default_handler_conf_->event_step_,
                              default_handler_conf_->additional_process_step_size_,
                              default_handler_conf_->store_type_.c_str());

                break;
            }
        }


    } catch (const libconfig::SettingNotFoundException& nfex) {
        roo::log_err("rpc.business.services not found!");
    } catch (std::exception& e) {
        roo::log_err("execptions catched for %s",  e.what());
    }


    // following specify handlers
    std::shared_ptr<HandlerType> handlers;
    {
        std::unique_lock<std::mutex> lock(lock_);
        handlers = handlers_;
    }

    int ret = 0;
    for (auto iter = handlers->begin(); iter != handlers->end(); ++iter) {
        roo::log_warning("update conf for: %s", iter->first.c_str());
        ret += iter->second->update_runtime_conf(conf);
    }

    return ret;
}

int EventRepos::module_status(std::string& strModule, std::string& strKey, std::string& strValue) {

    strModule = "EventRepos";
    strKey    = "eventrepos";

    std::stringstream ss;
    std::vector<std::string> total_services;
    std::vector<std::string> total_metrics;

    if (get_services("1.0.0", total_services) == 0) {
        for (size_t i = 0; i < total_services.size(); ++i) {
            ss << "service: " << total_services[i] << std::endl;
            ss << "metrics: " << std::endl;
            ss << "\t";

            total_metrics.clear();
            if (get_metrics("1.0.0", total_services[i], total_metrics) == 0) {
                for (size_t j = 0; j < total_metrics.size(); ++j) {
                    ss << total_metrics[j] << ", ";
                }
            }
            ss << std::endl << std::endl;
        }
    }

    strValue = ss.str();
    return 0;
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
            roo::log_err("create handler %s:%s failed!",
                         service.c_str(), entity_idx.c_str());
            return -1;
        }

        (*handlers_)[identity] = new_handler;
        handler = new_handler;

        return 0;
    }
}


int EventRepos::find_event_handler(const std::string& service,
                                   std::shared_ptr<EventHandler>& handler) {

    if (service.empty()) {
        roo::log_err("required service empty.");
        return -1;
    }

    std::shared_ptr<HandlerType> handlers;
    {
        std::unique_lock<std::mutex> lock(lock_);
        handlers = handlers_;
    }

    SAFE_ASSERT(handlers);

    for (auto iter = handlers->begin(); iter != handlers->end(); ++iter) {
        if (::strncmp(iter->first.c_str(), service.c_str(), service.size()) == 0) {
            handler = iter->second;
            return 0;
        }
    }

    roo::log_err("handler for service %s not found!", service.c_str());
    return -1;
}

int EventRepos::get_service_conf(const std::string& service, EventHandlerConf& handler_conf) {

    std::shared_ptr<EventHandler> handler;
    if (find_event_handler(service, handler) != 0) {
        return -1;
    }

    return handler->get_handler_conf(handler_conf);
}


EventHandlerConf EventRepos::get_default_handler_conf() {
    return *default_handler_conf_;
}


