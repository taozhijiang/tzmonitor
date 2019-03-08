/*-
 * Copyright (c) 2019 TAO Zhijiang<taozhijiang@gmail.com>
 *
 * Licensed under the BSD-3-Clause license, see LICENSE for full information.
 *
 */


#include <Utils/Log.h>

#include <Core/ProtoBuf.h>

#include <Business/EventTypes.h>
#include <Business/EventRepos.h>
#include <Business/EventHandler.h>


#include <RPC/RpcInstance.h>

#include <Protocol/gen-cpp/MonitorTask.pb.h>

#include "MonitorTaskService.h"

namespace tzrpc {

bool MonitorTaskService::init() {

    auto conf_ptr = ConfHelper::instance().get_conf();
    if(!conf_ptr) {
        log_err("ConfHelper not initialized? return conf_ptr empty!!!");
        return false;
    }

    bool init_success = false;

    try
    {
        const libconfig::Setting& rpc_services = conf_ptr->lookup("rpc.services");

        for(int i = 0; i < rpc_services.getLength(); ++i) {

            const libconfig::Setting& service = rpc_services[i];
            std::string instance_name;
            service.lookupValue("instance_name", instance_name);
            if (instance_name.empty()) {
                log_err("check service conf, required instance_name not found, skip this one.");
                continue;
            }

            log_debug("detected instance_name: %s", instance_name.c_str());

            // 发现是匹配的，则找到对应虚拟主机的配置文件了
            if (instance_name == instance_name_) {
                if (!handle_rpc_service_conf(service)) {
                    log_err("handle detail conf for %s failed.", instance_name.c_str());
                    return false;
                }

                log_debug("handle detail conf for host %s success!", instance_name.c_str());
                // OK
                init_success = true;
                break;
            }
        }

    } catch (const libconfig::SettingNotFoundException &nfex) {
        log_err("rpc.services not found!");
    } catch (std::exception& e) {
        log_err("execptions catched for %s",  e.what());
    }


    if(!init_success) {
        log_err("host %s init failed, may not configure for it?", instance_name_.c_str());
    }
    return init_success;
}

// 系统启动时候初始化，持有整个锁进行
bool MonitorTaskService::handle_rpc_service_conf(const libconfig::Setting& setting) {

    std::unique_lock<std::mutex> lock(conf_lock_);

    if (!conf_ptr_) {
        conf_ptr_.reset(new DetailExecutorConf());
        if (!conf_ptr_) {
            log_err("create DetailExecutorConf instance failed.");
            return false;
        }
    }

    ExecutorConf conf;
    if (RpcServiceBase::handle_rpc_service_conf(setting, conf) != 0) {
        log_err("Handler ExecutorConf failed.");
        return -1;
    }

    // 保存更新
    conf_ptr_->executor_conf_ = conf;

    // other confs may handle here...

    return true;
}



ExecutorConf MonitorTaskService::get_executor_conf() {
    SAFE_ASSERT(conf_ptr_);
    return conf_ptr_->executor_conf_;
}

int MonitorTaskService::module_runtime(const libconfig::Config& conf) {

    try
    {
        const libconfig::Setting& rpc_services = conf.lookup("rpc.services");
        for(int i = 0; i < rpc_services.getLength(); ++i) {

            const libconfig::Setting& service = rpc_services[i];
            std::string instance_name;
            service.lookupValue( "instance_name", instance_name);

            // 发现是匹配的，则找到对应虚拟主机的配置文件了
            if (instance_name == instance_name_) {
                log_notice("about to handle_rpc_service_runtime_conf update for %s", instance_name_.c_str());
                return handle_rpc_service_runtime_conf(service);
            }
        }

    } catch (const libconfig::SettingNotFoundException &nfex) {
        log_err("rpc.services not found!");
    } catch (std::exception& e) {
        log_err("execptions catched for %s",  e.what());
    }

    log_err("conf for %s not found!!!!", instance_name_.c_str());
    return -1;
}

// 做一些可选的配置动态更新
bool MonitorTaskService::handle_rpc_service_runtime_conf(const libconfig::Setting& setting) {

    ExecutorConf conf;
    if (RpcServiceBase::handle_rpc_service_conf(setting, conf) != 0) {
        log_err("Handler ExecutorConf failed.");
        return -1;
    }

    {
        // do swap here
        std::unique_lock<std::mutex> lock(conf_lock_);
        conf_ptr_->executor_conf_ = conf;
    }

    return 0;
}

int MonitorTaskService::module_status(std::string& strModule, std::string& strKey, std::string& strValue) {

    // empty status ...

    return 0;
}


void MonitorTaskService::handle_RPC(std::shared_ptr<RpcInstance> rpc_instance) {

    using MonitorTask::OpCode;

    // Call the appropriate RPC handler based on the request's opCode.
    switch (rpc_instance->get_opcode()) {
        case OpCode::CMD_READ_EVENT:
            read_ops_impl(rpc_instance);
            break;
        case OpCode::CMD_WRIT_EVENT:
            write_ops_impl(rpc_instance);
            break;

        default:
            log_err("Received RPC request with unknown opcode %u: "
                    "rejecting it as invalid request",
                    rpc_instance->get_opcode());
            rpc_instance->reject(RpcResponseStatus::INVALID_REQUEST);
    }
}


void MonitorTaskService::read_ops_impl(std::shared_ptr<RpcInstance> rpc_instance) {

    // 再做一次opcode校验
    RpcRequestMessage& rpc_request_message = rpc_instance->get_rpc_request_message();
    if (rpc_request_message.header_.opcode != MonitorTask::OpCode::CMD_READ_EVENT) {
        log_err("invalid opcode %u in service MonitorTask.", rpc_request_message.header_.opcode);
        rpc_instance->reject(RpcResponseStatus::INVALID_REQUEST);
        return;
    }

    MonitorTask::MonitorReadOps::Response response;
    response.set_code(0);
    response.set_desc("OK");

    do {

        // 消息体的unmarshal
        MonitorTask::MonitorReadOps::Request request;
        if (!ProtoBuf::unmarshalling_from_string(rpc_request_message.payload_, &request)) {
            log_err("unmarshal request failed.");
            response.set_code(-1);
            response.set_desc("unmarshalling failed.");
            break;
        }

        log_debug("ReadRequest: %s", ProtoBuf::dump(request).c_str());

        // 相同类目下的子RPC调用分发
        if (request.has_ping()) {
            log_debug("MonitorTask::MonitorReadOps::ping -> %s", request.ping().msg().c_str());
            response.mutable_ping()->set_msg("[[[pong]]]");
            break;
        } else if (request.has_select()) {

            event_cond_t cond {};

            cond.version = request.select().version();
            cond.tm_interval = request.select().tm_interval();
            cond.service = request.select().service();
            cond.metric = request.select().metric();
            cond.tm_start = request.select().tm_start();
            cond.entity_idx = request.select().entity_idx();
            cond.tag = request.select().tag();

            std::string groupby = request.select().groupby();
            if (groupby == "tag") {
                cond.groupby = GroupType::kGroupbyTag;
            } else if(groupby == "timestamp") {
                cond.groupby = GroupType::kGroupbyTimestamp;
            } else {
                cond.groupby = GroupType::kGroupNone;
            }

            event_select_t stat {};
            int ret = EventRepos::instance().get_event(cond, stat);
            if (ret != 0) {
                log_err("call select return: %d",  ret);
                response.set_code(ret);
                response.set_desc("get_event error");
                return;
            }

            response.set_code(0);
            response.set_desc("OK");

            response.mutable_select()->set_version(cond.version);
            response.mutable_select()->set_service(stat.service);
            response.mutable_select()->set_timestamp(stat.timestamp);
            response.mutable_select()->set_metric(stat.metric);
            response.mutable_select()->set_tm_interval(stat.tm_interval);
            response.mutable_select()->set_tm_start(stat.timestamp);
            response.mutable_select()->set_entity_idx(stat.entity_idx);
            response.mutable_select()->set_tag(stat.tag);


            response.mutable_select()->mutable_summary()->set_timestamp(stat.summary.timestamp);
            response.mutable_select()->mutable_summary()->set_tag(stat.summary.tag);
            response.mutable_select()->mutable_summary()->set_count(stat.summary.count);
            response.mutable_select()->mutable_summary()->set_value_sum(stat.summary.value_sum);
            response.mutable_select()->mutable_summary()->set_value_avg(stat.summary.value_avg);
            response.mutable_select()->mutable_summary()->set_value_min(stat.summary.value_min);
            response.mutable_select()->mutable_summary()->set_value_max(stat.summary.value_max);
            response.mutable_select()->mutable_summary()->set_value_p10(stat.summary.value_p10);
            response.mutable_select()->mutable_summary()->set_value_p50(stat.summary.value_p50);
            response.mutable_select()->mutable_summary()->set_value_p90(stat.summary.value_p90);

            for (auto iter = stat.info.begin(); iter != stat.info.end(); ++iter) {
                auto item = response.mutable_select()->add_info();

                item->set_timestamp(iter->timestamp);
                item->set_tag(iter->tag);

                item->set_count(iter->count);
                item->set_value_sum(iter->value_sum);
                item->set_value_avg(iter->value_avg);
                item->set_value_min(iter->value_min);
                item->set_value_max(iter->value_max);
                item->set_value_p10(iter->value_p10);
                item->set_value_p50(iter->value_p50);
                item->set_value_p90(iter->value_p90);
            }

            break;

        } else if (request.has_metrics()) {

            std::string version = request.metrics().version();
            std::string service = request.metrics().service();

            std::vector<std::string> metric_stat;


            int ret = EventRepos::instance().get_metrics(version, service, metric_stat);
            if (ret != 0) {
                log_err("call metrics return: %d",  ret);
                response.set_code(ret);
                response.set_desc("get_metrics failed.");
                response.mutable_metrics()->set_version(version);
                response.mutable_metrics()->set_service(service);
                return;
            }

            response.set_code(0);
            response.set_desc("OK");
            response.mutable_metrics()->set_version(version);
            response.mutable_metrics()->set_service(service);

            EventHandlerConf handler_conf;
            ret = EventRepos::instance().get_service_conf(service, handler_conf);
            if (ret == 0) {
                response.mutable_metrics()->set_event_step(handler_conf.event_step_);
                response.mutable_metrics()->set_event_linger(handler_conf.event_linger_);
                response.mutable_metrics()->set_store_type(handler_conf.store_type_);
            }

            for (auto iter = metric_stat.begin(); iter != metric_stat.end(); ++ iter) {
                response.mutable_metrics()->add_metric(*iter);
            }

            break;

        } else if (request.has_services()) {

            std::string version = request.services().version();
            std::vector<std::string> service_stat;

            int ret = EventRepos::instance().get_services(version, service_stat);
            if (ret != 0) {
                log_err("call metrics return: %d",  ret);
                response.set_code(ret);
                response.set_desc("get_services failed.");
                response.mutable_services()->set_version(version);
                return;
            }

            response.set_code(0);
            response.set_desc("OK");
            response.mutable_services()->set_version(version);

            for (auto iter = service_stat.begin(); iter != service_stat.end(); ++iter) {
                response.mutable_services()->add_service(*iter);
            }

            break;

        } else {
            log_err("undetected specified service call.");
            rpc_instance->reject(RpcResponseStatus::INVALID_REQUEST);
            return;
        }

    } while (0);

    log_debug("ReadRequest: return\n%s", ProtoBuf::dump(response).c_str());

    std::string response_str;
    ProtoBuf::marshalling_to_string(response, &response_str);
    rpc_instance->reply_rpc_message(response_str);
}

void MonitorTaskService::write_ops_impl(std::shared_ptr<RpcInstance> rpc_instance) {

       // 再做一次opcode校验
    RpcRequestMessage& rpc_request_message = rpc_instance->get_rpc_request_message();
    if (rpc_request_message.header_.opcode != MonitorTask::OpCode::CMD_WRIT_EVENT) {
        log_err("invalid opcode %u in service MonitorTask.", rpc_request_message.header_.opcode);
        rpc_instance->reject(RpcResponseStatus::INVALID_REQUEST);
        return;
    }

    MonitorTask::MonitorWriteOps::Response response;
    response.set_code(0);
    response.set_desc("OK");

    do {

        // 消息体的unmarshal
        MonitorTask::MonitorWriteOps::Request request;
        if (!ProtoBuf::unmarshalling_from_string(rpc_request_message.payload_, &request)) {
            log_err("unmarshal request failed.");
            response.set_code(-1);
            response.set_desc("unmarshalling failed");
            break;
        }

        log_debug("WriteRequest: %s", ProtoBuf::dump(request).c_str());

        // 相同类目下的子RPC调用分发
        if (request.has_report()) {

            event_report_t report{};
            report.version = request.report().version();
            report.timestamp = request.report().timestamp();
            report.service = request.report().service();
            report.entity_idx = request.report().entity_idx();

            int size = request.report().data_size();
            for (int i=0; i<size; ++i) {
                event_data_t item {};

                auto p_data = request.report().data(i);
                item.msgid = p_data.msgid();
                item.metric = p_data.metric();
                item.tag = p_data.tag();
                item.value = p_data.value();

                report.data.emplace_back(item);
            }

            auto ret = EventRepos::instance().add_event(report);
            if (ret != 0) {
                log_err("add event failed with return: %d", ret);
                response.set_code(ret);
                response.set_desc("add_event failed.");
            }

            break;

        } else {
            log_err("undetected specified service call.");
            rpc_instance->reject(RpcResponseStatus::INVALID_REQUEST);
            return;
        }

    } while (0);

    log_debug("WriteRequest: return\n%s", ProtoBuf::dump(response).c_str());

    std::string response_str;
    ProtoBuf::marshalling_to_string(response, &response_str);
    rpc_instance->reply_rpc_message(response_str);
}


} // namespace tzrpc
