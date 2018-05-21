#include "General.h"
#include "ErrorDef.h"
#include "TzMonitorService.h"

#include <core/EventItem.h>
#include <core/EventRepos.h>

#include <utils/Log.h>


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
        EventSql::ev_cond_t cond {};
        cond.version = req.version;
        cond.host = req.host;
        cond.serv = req.serv;
        cond.entity_idx = req.entity_idx;
        cond.name = req.name;
        cond.flag = req.flag;
        cond.start = req.start;
        cond.interval_sec = req.interval_sec;

        EventSql::ev_stat_t stat {};
        if (EventRepos::instance().get_event(cond, stat) != ErrorDef::OK) {
            log_err("call get_event failed!");
            break;
        }

        resp.__set_name(cond.name);
        resp.__set_flag(cond.flag);

        resp.__set_version("1.0.0");
        resp.__set_time(stat.time);

        tz_thrift::ev_data_info_t info {};
        info.__set_count(stat.count);
        info.__set_value_sum(stat.value_sum);
        info.__set_value_avg(stat.value_avg);
        info.__set_value_std(stat.value_std);

        resp.__set_info(info);
        resp.result.__set_code(0);
        resp.result.__set_desc("OK");

        return;

    } while (0);

    resp.result.__set_code(-1);
    resp.result.__set_desc("ERROR");
}


void TzMonitorHandler::ev_query_detail(tz_thrift::ev_query_response_detail_t& resp, const tz_thrift::ev_query_request_t& req) override {

    do {
        EventSql::ev_cond_t cond {};
        cond.version = req.version;
        cond.host = req.host;
        cond.serv = req.serv;
        cond.entity_idx = req.entity_idx;
        cond.name = req.name;
        cond.flag = req.flag;
        cond.start = req.start;
        cond.interval_sec = req.interval_sec;

        EventSql::ev_stat_detail_t stat {};
        if (EventRepos::instance().get_event(cond, stat) != ErrorDef::OK) {
            log_err("call get_event failed!");
            break;
        }

        resp.__set_name(cond.name);
        resp.__set_flag(cond.flag);

        resp.__set_version("1.0.0");
        resp.__set_time(stat.time);

        std::vector<tz_thrift::ev_data_info_t> info {};
        for (auto iter = stat.info.begin(); iter != stat.info.end(); ++iter) {
            tz_thrift::ev_data_info_t item {};

            item.__set_time(iter->time);
            item.__set_count(iter->count);
            item.__set_value_sum(iter->value_sum);
            item.__set_value_avg(iter->value_avg);
            item.__set_value_std(iter->value_std);

            info.push_back(item);
        }

        resp.__set_info(info);
        resp.result.__set_code(0);
        resp.result.__set_desc("OK");

        return;

    } while (0);

    resp.result.__set_code(-1);
    resp.result.__set_desc("ERROR");
}
