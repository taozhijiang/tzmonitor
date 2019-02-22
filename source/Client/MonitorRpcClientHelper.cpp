/*-
 * Copyright (c) 2018 TAO Zhijiang<taozhijiang@gmail.com>
 *
 * Licensed under the BSD-3-Clause license, see LICENSE for full information.
 *
 */


#include <boost/noncopyable.hpp>


#include <Client/LogClient.h>

#include <Client/ProtoBuf.h>
#include <Client/Common.h>
#include <Client/RpcClient.h>

#include <Client/MonitorTask.pb.h>

#include <Client/MonitorRpcClientHelper.h>

namespace tzmonitor_client {


class MonitorRpcClientHelper::Impl : private boost::noncopyable {

public:
    Impl(const std::string& ip, uint16_t port):
        ip_(ip), port_(port) {
    }

    int rpc_event_submit(const event_report_t& report) {


        if (report.version.empty() ||
            report.service.empty() || report.entity_idx.empty() ||
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

        if (!response.has_code() || response.has_code() != 0) {
            log_err("response return failed.");
            if (response.has_code() && response.has_desc()) {
                log_err("error info: %d %s", response.code(), response.desc().c_str());
            }

            return -1;
        }

        return 0;

    }


    int rpc_event_select(const event_cond_t& cond, event_select_t& resp_info) {

#if 0
        do {

            if (cond.version.empty() || cond.name.empty() ||  cond.interval_sec <= 0 ) {
                log_err("thrift query param check error!");
                return ErrorDef::ParamErr;
            }

            tz_thrift::ev_query_request_t req {};
            req.version = cond.version;
            req.name = cond.name;
            req.interval_sec = cond.interval_sec;
            if (cond.groupby == GroupType::kGroupbyTime) {
                req.__set_groupby("time");
            } else if (cond.groupby == GroupType::kGroupbyFlag) {
                req.__set_groupby("flag");
            }
            req.__set_host(cond.host);
            req.__set_serv(cond.serv);
            req.__set_entity_idx(cond.entity_idx);
            req.__set_flag(cond.flag);

            tz_thrift::ev_query_response_t resp {};
            int ret_code = TThriftClient::call_service<TzMonitorClient>(ip_, static_cast<uint16_t>(port_),
                                        &TzMonitorClient::ev_query, std::ref(resp), std::cref(req));

            if (ret_code == 0) {

                if( resp.result.code == 0 && resp.result.desc == "OK") {

                    // empty string equals
                    if (resp.version != req.version || resp.name != req.name ||
                        resp.interval_sec != req.interval_sec ||
                        resp.host != req.host || resp.serv != req.serv ||
                        resp.entity_idx != req.entity_idx || resp.flag != req.flag )
                    {
                        log_err("thrift return does not match request param.");
                        return ErrorDef::CheckErr;
                    }

                    resp_info.version = resp.version;
                    resp_info.time = resp.time;
                    resp_info.host = resp.host;
                    resp_info.serv = resp.serv;
                    resp_info.entity_idx = resp.entity_idx;
                    resp_info.name = resp.name;
                    resp_info.flag = resp.flag;

                    resp_info.summary.count = resp.summary.count;
                    resp_info.summary.value_sum = resp.summary.value_sum;
                    resp_info.summary.value_avg = resp.summary.value_avg;
                    resp_info.summary.value_std = resp.summary.value_std;


                    if (cond.groupby != GroupType::kGroupNone) {

                        std::vector<event_info_t> info;
                        for (auto iter = resp.info.begin(); iter != resp.info.end(); ++iter) {

                            event_info_t item;
                            if (cond.groupby == GroupType::kGroupbyTime) {
                                item.time = iter->time;
                            } else if (cond.groupby == GroupType::kGroupbyFlag) {
                                item.flag = iter->flag;
                            }

                            item.count = iter->count;
                            item.value_sum = iter->value_sum;
                            item.value_avg = iter->value_avg;
                            item.value_std = iter->value_std;

                            info.push_back(item);
                        }

                        // collect it
                        resp_info.info = std::move(info);
                    }

                    return ErrorDef::OK;
                }

                // request handler error
                log_err("call thrift logic return: %d: %s", resp.result.code, resp.result.desc.c_str());
                return resp.result.code != 0 ? resp.result.code : ErrorDef::ThriftErr;
            }

        } while (0);

        log_err("Call ThriftService TzMonitorClient::ev_query @ %s:%d failed!", ip_.c_str(), port_);
        return ErrorDef::ThriftErr;
#endif

        return 0;
    }

private:
    std::string ip_;
    uint16_t    port_;

    std::unique_ptr<RpcClient> rpc_client_;
};



// call forward

MonitorRpcClientHelper::MonitorRpcClientHelper(const std::string& ip, uint16_t port) {
    impl_ptr_.reset(new Impl(ip, port));
     if (!impl_ptr_) {
         log_err("create impl failed, CRITICAL!!!!");
         ::abort();
     }
}

MonitorRpcClientHelper::~MonitorRpcClientHelper(){
}

int MonitorRpcClientHelper::rpc_event_submit(const event_report_t& report) {
    return impl_ptr_->rpc_event_submit(report);
}

int MonitorRpcClientHelper::rpc_event_select(const event_cond_t& cond, event_select_t& resp) {
    return impl_ptr_->rpc_event_select(cond, resp);
}




} // end namespace tzmonitor_client
