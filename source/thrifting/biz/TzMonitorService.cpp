#include "General.h"
#include "ErrorDef.h"

#include <boost/algorithm/string.hpp>

#include <core/EventItem.h>
#include <core/EventRepos.h>
#include <utils/Log.h>

#include "TzMonitorService.h"

void TzMonitorHandler::ping_test(tz_thrift::result_t& _return, const tz_thrift::ping_t& req) override {

    log_debug("ping_test recv: %s", req.msg.c_str());

    _return.__set_code(0);
    _return.__set_desc("OK");
}


void TzMonitorHandler::ev_submit(tz_thrift::result_t& _return, const tz_thrift::ev_report_t& req) override {

    log_debug("ev_submit from: %s", req.host.c_str());

    event_report_t events {};
    events.version = req.version;
    events.time = req.time;
    events.host = req.host;
    events.serv = req.serv;
    events.entity_idx = req.entity_idx;

    for (size_t i = 0; i < req.data.size(); i++) {
        event_data_t item{};
        item.name = req.data[i].name;
        item.msgid = req.data[i].msgid;
        item.value = req.data[i].value;
        item.flag = req.data[i].flag;

        events.data.push_back(item);
    }

    if (EventRepos::instance().add_event(events) == ErrorDef::OK) {
        _return.__set_code(0);
        _return.__set_desc("OK");
        return;
    }

    _return.__set_code(-1);
    _return.__set_desc("ERROR");
}


void TzMonitorHandler::ev_query (tz_thrift::ev_query_response_t& resp, const tz_thrift::ev_query_request_t& req) override {

    do {
        event_cond_t cond {};

        cond.version = req.version;
        cond.host = req.host;
        cond.serv = req.serv;
        cond.entity_idx = req.entity_idx;
        cond.name = req.name;
        cond.flag = req.flag;
        cond.start = req.start;
        cond.interval_sec = req.interval_sec;
        cond.groupby = GroupType::kGroupNone;
        if (boost::iequals(req.groupby, "time")) {
            cond.groupby = GroupType::kGroupbyTime;
        } else if (boost::iequals(req.groupby, "flag")) {
            cond.groupby = GroupType::kGroupbyFlag;
        }

        event_query_t stat{};
        if (EventRepos::instance().get_event(cond, stat) != ErrorDef::OK) {
            log_err("call get_event failed!");
            break;
        }

        resp.__set_name(cond.name);
        resp.__set_flag(cond.flag);

        resp.__set_version("1.0.0");
        resp.__set_time(stat.time);
        resp.__set_interval_sec(cond.interval_sec);

        // 总览统计数据
        tz_thrift::ev_info_t summary_item {};
        summary_item.__set_count(stat.summary.count);
        summary_item.__set_value_sum(stat.summary.value_sum);
        summary_item.__set_value_avg(stat.summary.value_avg);
        summary_item.__set_value_std(stat.summary.value_std);
        resp.__set_summary(summary_item);

        if (cond.groupby != GroupType::kGroupNone) {

            std::vector<tz_thrift::ev_info_t> info{};
            for (auto iter = stat.info.begin(); iter != stat.info.end(); ++iter) {
                tz_thrift::ev_info_t item {};

                if ( cond.groupby == GroupType::kGroupbyTime ) {
                    item.__set_time(iter->time);
                } else if ( cond.groupby == GroupType::kGroupbyFlag ) {
                    item.__set_flag(iter->flag);
                }

                item.__set_count(iter->count);
                item.__set_value_sum(iter->value_sum);
                item.__set_value_avg(iter->value_avg);
                item.__set_value_std(iter->value_std);

                info.push_back(item);
            }
            resp.__set_info(info);
        }

        resp.result.__set_code(0);
        resp.result.__set_desc("OK");

        return;

    } while (0);

    resp.result.__set_code(-1);
    resp.result.__set_desc("ERROR");
}
