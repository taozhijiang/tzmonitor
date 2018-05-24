#include "../helper/TThriftTypes.h"
#include "../helper/TThriftClient.h"

#include "TzMonitorService.h"
#include "TzMonitorThriftClientHelper.h"


class TzMonitorThriftClientHelper::Impl : private boost::noncopyable {
public:
    Impl(const std::string& ip, uint16_t port):
        ip_(ip), port_(port) {
    }

    int thrift_event_submit(const event_report_t& report) {

        do {

            tz_thrift::ev_report_t req {};
            std::vector<tz_thrift::ev_data_t> orders {};

            req.version = report.version;
            req.host = report.host;
            req.serv = report.serv;
            req.entity_idx = report.entity_idx;
            req.time = report.time;

            for (auto iter = report.data.cbegin(); iter != report.data.cend(); ++iter) {
                tz_thrift::ev_data_t item {};
                item.name = iter->name;
                item.msgid = iter->msgid;
                item.value = iter->value;
                item.flag = iter->flag;

                orders.push_back(item);
            }

            req.data = orders;

            tz_thrift::result_t result {};

            int ret_code = TThriftClient::call_service<TzMonitorClient> (ip_, static_cast<uint16_t>(port_),
                                        &TzMonitorClient::ev_submit, std::ref(result), std::cref(req));

            if (ret_code == 0 && result.code == 0 && result.desc == "OK") {
                return ErrorDef::OK;
            }

        } while (0);

        return ErrorDef::ThriftErr;
    }


    int thrift_event_query(const event_cond_t& cond, event_query_t& resp_info) {

        do {

            tz_thrift::ev_query_request_t req {};
            req.version = cond.version;
            req.name = cond.name;
            req.interval_sec = cond.interval_sec;
            if (cond.groupby == GroupType::kGroupbyTime) {
                req.__set_groupby("time");
            } else if (cond.groupby == GroupType::kGroupbyFlag) {
                req.__set_groupby("flag");
            }
            req.__set_host(req.host);
            req.__set_serv(req.serv);
            req.__set_entity_idx(req.entity_idx);
            req.__set_flag(req.flag);

            tz_thrift::ev_query_response_t resp {};
            int ret_code = TThriftClient::call_service<TzMonitorClient>(ip_, static_cast<uint16_t>(port_),
                                        &TzMonitorClient::ev_query, std::ref(resp), std::cref(req));

            if (ret_code == 0 && resp.result.code == 0 && resp.result.desc == "OK") {

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

        } while (0);

        return ErrorDef::ThriftErr;
    }

private:
    std::string ip_;
    uint16_t    port_;
};



// call forward

TzMonitorThriftClientHelper::TzMonitorThriftClientHelper(const std::string& ip, uint16_t port) {
    impl_ptr_.reset(new Impl(ip, port));
     if (!impl_ptr_) {
         log_err("create impl failed, CRITICAL!!!!");
         ::abort();
     }
}

TzMonitorThriftClientHelper::~TzMonitorThriftClientHelper(){
}


int TzMonitorThriftClientHelper::thrift_event_submit(const event_report_t& report) {
    return impl_ptr_->thrift_event_submit(report);
}

int TzMonitorThriftClientHelper::thrift_event_query(const event_cond_t& cond, event_query_t& resp) {
    return impl_ptr_->thrift_event_query(cond, resp);
}
