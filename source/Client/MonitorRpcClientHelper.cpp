/*-
 * Copyright (c) 2019 TAO Zhijiang<taozhijiang@gmail.com>
 *
 * Licensed under the BSD-3-Clause license, see LICENSE for full information.
 *
 */




#include <Client/LogClient.h>

#include <Client/ProtoBuf.h>
#include <Client/Common.h>
#include <Client/RpcClient.h>

#include <Client/MonitorTask.pb.h>
#include <Client/MonitorRpcClientHelper.h>

namespace tzmonitor_client {


int MonitorRpcClientHelper::rpc_ping() {

    tzrpc::MonitorTask::MonitorReadOps::Request request;
    request.mutable_ping()->set_msg("ping");

    std::string mar_str;
    if(!tzrpc::ProtoBuf::marshalling_to_string(request, &mar_str)) {
        log_err("marshalling message failed.");
        return -1;
    }

    if (!rpc_client_) {
        rpc_client_.reset(new RpcClient(ip_, port_));
        if (!rpc_client_) {
            log_err("create rpc client failed.");
            return -1;
        }
    }

    std::string response_str;
    auto status = rpc_client_->call_RPC(tzrpc::ServiceID::MONITOR_TASK_SERVICE,
                                        tzrpc::MonitorTask::OpCode::CMD_READ_EVENT,
                                        mar_str, response_str);

    if (status != RpcClientStatus::OK) {
        log_err("rpc call return code %d", static_cast<uint8_t>(status) );
        return -1;
    }

    tzrpc::MonitorTask::MonitorReadOps::Response response;
    if(!tzrpc::ProtoBuf::unmarshalling_from_string(response_str, &response)) {
        log_err("unmarshalling message failed.");
        return -1;
    }

    if (!response.has_code() || response.code() != 0) {
        log_err("response return failed.");
        if (response.has_code() && response.has_desc()) {
            log_err("error info: %d %s", response.code(), response.desc().c_str());
        }

        return -1;
    }

    // todo 校验提交返回参数

    std::string rsp_str = response.ping().msg();
    log_debug("ping test return: %s",  rsp_str.c_str());

    if (rsp_str == "[[[pong]]]") {
        return 0;
    }

    return -1;
}

int MonitorRpcClientHelper::rpc_event_submit(const event_report_t& report) {


    if (report.version.empty() || report.service.empty() ||
        report.timestamp <= 0 ){
        log_err("submit param check error!");
        return -1;
    }

    tzrpc::MonitorTask::MonitorWriteOps::Request request;
    request.mutable_report()->set_version(report.version);
    request.mutable_report()->set_timestamp(report.timestamp);
    request.mutable_report()->set_service(report.service);
    request.mutable_report()->set_entity_idx(report.entity_idx);

    for (auto iter = report.data.cbegin(); iter != report.data.cend(); ++iter) {
        //tzrpc::MonitorTask::MonitorWriteOps::Request::ev_data_t* item = request.mutable_report()->add_data();
        auto item = request.mutable_report()->add_data();
        item->set_msgid(iter->msgid);
        item->set_metric(iter->metric);
        item->set_value(iter->value);
        item->set_tag(iter->tag);
    }

    std::string mar_str;
    if(!tzrpc::ProtoBuf::marshalling_to_string(request, &mar_str)) {
        log_err("marshalling message failed.");
        return -1;
    }

    log_notice("report count %lu with marshal size: %lu", report.data.size(), mar_str.size());

    if (!rpc_client_) {
        rpc_client_.reset(new RpcClient(ip_, port_));
        if (!rpc_client_) {
            log_err("create rpc client failed.");
            return -1;
        }
    }

    std::string response_str;
    auto status = rpc_client_->call_RPC(tzrpc::ServiceID::MONITOR_TASK_SERVICE,
                                        tzrpc::MonitorTask::OpCode::CMD_WRIT_EVENT,
                                        mar_str, response_str);

    if (status != RpcClientStatus::OK) {
        log_err("rpc call return code %d", static_cast<uint8_t>(status) );
        return -1;
    }

    tzrpc::MonitorTask::MonitorWriteOps::Response response;
    if(!tzrpc::ProtoBuf::unmarshalling_from_string(response_str, &response)) {
        log_err("unmarshalling message failed.");
        return -1;
    }

    if (!response.has_code() || response.code() != 0) {
        log_err("response return failed.");
        if (response.has_code() && response.has_desc()) {
            log_err("error info: %d %s", response.code(), response.desc().c_str());
        }

        return -1;
    }

    return 0;

}


int MonitorRpcClientHelper::rpc_event_select(const event_cond_t& cond, event_select_t& resp_info) {

    if (cond.version.empty() || cond.service.empty() ||
        cond.metric.empty()  || cond.tm_interval < 0) {
        log_err("select param check error!");
        return -1;
    }

    tzrpc::MonitorTask::MonitorReadOps::Request request;
    request.mutable_select()->set_version(cond.version);
    request.mutable_select()->set_service(cond.service);
    request.mutable_select()->set_metric(cond.metric);

    request.mutable_select()->set_tm_interval(cond.tm_interval);
    request.mutable_select()->set_tm_start(cond.tm_start);
    request.mutable_select()->set_entity_idx(cond.entity_idx);
    request.mutable_select()->set_tag(cond.tag);

    request.mutable_select()->set_groupby(static_cast<int32_t>(cond.groupby));
    request.mutable_select()->set_orderby(static_cast<int32_t>(cond.orderby));
    request.mutable_select()->set_orders(static_cast<int32_t>(cond.orders));
    request.mutable_select()->set_limit(static_cast<int32_t>(cond.limit));


    std::string mar_str;
    if(!tzrpc::ProtoBuf::marshalling_to_string(request, &mar_str)) {
        log_err("marshalling message failed.");
        return -1;
    }

    if (!rpc_client_) {
        rpc_client_.reset(new RpcClient(ip_, port_));
        if (!rpc_client_) {
            log_err("create rpc client failed.");
            return -1;
        }
    }

    std::string response_str;
    auto status = rpc_client_->call_RPC(tzrpc::ServiceID::MONITOR_TASK_SERVICE,
                                        tzrpc::MonitorTask::OpCode::CMD_READ_EVENT,
                                        mar_str, response_str);

    if (status != RpcClientStatus::OK) {
        log_err("rpc call return code %d", static_cast<uint8_t>(status) );
        return -1;
    }

    tzrpc::MonitorTask::MonitorReadOps::Response response;
    if(!tzrpc::ProtoBuf::unmarshalling_from_string(response_str, &response)) {
        log_err("unmarshalling message failed.");
        return -1;
    }

    if (!response.has_code() || response.code() != 0) {
        log_err("response return failed.");
        if (response.has_code() && response.has_desc()) {
            log_err("error info: %d %s", response.code(), response.desc().c_str());
        }

        return -1;
    }

    // todo 校验提交返回参数

    resp_info.version = response.select().version();
    resp_info.service = response.select().service();
    resp_info.timestamp = response.select().timestamp();
    resp_info.metric = response.select().metric();
    resp_info.tm_interval = response.select().tm_interval();
    resp_info.entity_idx = response.select().entity_idx();
    resp_info.tag = response.select().tag();

    resp_info.summary.count = response.select().summary().count();
    resp_info.summary.value_sum = response.select().summary().value_sum();
    resp_info.summary.value_avg = response.select().summary().value_avg();
    resp_info.summary.value_min = response.select().summary().value_min();
    resp_info.summary.value_max = response.select().summary().value_max();
    resp_info.summary.value_p10 = response.select().summary().value_p10();
    resp_info.summary.value_p50 = response.select().summary().value_p50();
    resp_info.summary.value_p90 = response.select().summary().value_p90();


    if (cond.groupby != GroupType::kGroupNone) {

        int size = response.select().info_size();
        std::vector<event_info_t> info;

        for (int i=0; i<size; ++i) {

            event_info_t item {};
            auto p_info = response.select().info(i);
            if (cond.groupby == GroupType::kGroupbyTimestamp) {
                item.timestamp = p_info.timestamp();
                item.tag = "";
            } else if (cond.groupby == GroupType::kGroupbyTag) {
                item.timestamp = 0;
                item.tag = p_info.tag();
            }

            item.count = p_info.count();
            item.value_sum = p_info.value_sum();
            item.value_avg = p_info.value_avg();
            item.value_min = p_info.value_min();
            item.value_max = p_info.value_max();
            item.value_p10 = p_info.value_p10();
            item.value_p50 = p_info.value_p50();
            item.value_p90 = p_info.value_p90();

            info.push_back(item);
        }

        // collect it
        resp_info.info = std::move(info);
    }


    return 0;
}

int MonitorRpcClientHelper::rpc_known_metrics(const std::string& version, const std::string& service,
                                              event_handler_conf_t& handler_conf, std::vector<std::string>& metrics) {

    if (service.empty()) {
        log_err("select metrics param check error!");
        return -1;
    }

    tzrpc::MonitorTask::MonitorReadOps::Request request;
    request.mutable_metrics()->set_version(version);
    request.mutable_metrics()->set_service(service);

    std::string mar_str;
    if(!tzrpc::ProtoBuf::marshalling_to_string(request, &mar_str)) {
        log_err("marshalling message failed.");
        return -1;
    }

    if (!rpc_client_) {
        rpc_client_.reset(new RpcClient(ip_, port_));
        if (!rpc_client_) {
            log_err("create rpc client failed.");
            return -1;
        }
    }

    std::string response_str;
    auto status = rpc_client_->call_RPC(tzrpc::ServiceID::MONITOR_TASK_SERVICE,
                                        tzrpc::MonitorTask::OpCode::CMD_READ_EVENT,
                                        mar_str, response_str);

    if (status != RpcClientStatus::OK) {
        log_err("rpc call return code %d", static_cast<uint8_t>(status) );
        return -1;
    }

    tzrpc::MonitorTask::MonitorReadOps::Response response;
    if(!tzrpc::ProtoBuf::unmarshalling_from_string(response_str, &response)) {
        log_err("unmarshalling message failed.");
        return -1;
    }

    if (!response.has_code() || response.code() != 0) {
        log_err("response return failed.");
        if (response.has_code() && response.has_desc()) {
            log_err("error info: %d %s", response.code(), response.desc().c_str());
        }

        return -1;
    }

    // todo 校验提交返回参数
    std::string r_service = response.metrics().service();
    if (r_service != service) {
        log_err("return param check error: service %s - %s",
                r_service.c_str(), service.c_str());
        return -1;
    }

    int size = response.metrics().metric_size();
    for (int i=0; i<size; ++i) {
        auto p_metric = response.metrics().metric(i);
        metrics.push_back(p_metric);
    }

    if (response.metrics().has_event_step()) {
        handler_conf.event_step_ = response.metrics().event_step();
        handler_conf.event_linger_ = response.metrics().event_linger();
        handler_conf.store_type_ = response.metrics().store_type();
    }

    return 0;
}


int MonitorRpcClientHelper::rpc_known_services(const std::string& version, std::vector<std::string>& services) {

    tzrpc::MonitorTask::MonitorReadOps::Request request;
    request.mutable_services()->set_version(version);

    std::string mar_str;
    if(!tzrpc::ProtoBuf::marshalling_to_string(request, &mar_str)) {
        log_err("marshalling message failed.");
        return -1;
    }

    if (!rpc_client_) {
        rpc_client_.reset(new RpcClient(ip_, port_));
        if (!rpc_client_) {
            log_err("create rpc client failed.");
            return -1;
        }
    }

    std::string response_str;
    auto status = rpc_client_->call_RPC(tzrpc::ServiceID::MONITOR_TASK_SERVICE,
                                        tzrpc::MonitorTask::OpCode::CMD_READ_EVENT,
                                        mar_str, response_str);

    if (status != RpcClientStatus::OK) {
        log_err("rpc call return code %d", static_cast<uint8_t>(status) );
        return -1;
    }

    tzrpc::MonitorTask::MonitorReadOps::Response response;
    if(!tzrpc::ProtoBuf::unmarshalling_from_string(response_str, &response)) {
        log_err("unmarshalling message failed.");
        return -1;
    }

    if (!response.has_code() || response.code() != 0) {
        log_err("response return failed.");
        if (response.has_code() && response.has_desc()) {
            log_err("error info: %d %s", response.code(), response.desc().c_str());
        }

        return -1;
    }

    int size = response.services().service_size();
    for (int i=0; i<size; ++i) {
        auto p_service = response.services().service(i);
        services.push_back(p_service);
    }

    return 0;
}

} // end namespace tzmonitor_client
