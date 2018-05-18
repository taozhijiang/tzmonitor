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
