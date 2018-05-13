#include "General.h"
#include "TzMonitorService.h"

#include <utils/Log.h>


void TzMonitorHandler::ping_test(tz_thrift::result_t& _return, const tz_thrift::ping_t& req) override {

    log_debug("ping_test recv: %s", req.msg.c_str());

    _return.__set_code(0);
    _return.__set_desc("OK");
}
