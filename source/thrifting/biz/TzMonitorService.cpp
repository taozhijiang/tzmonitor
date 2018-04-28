#include "General.h"
#include "TzMonitorService.h"


void TzMonitorHandler::ping_test(tz_thrift::result_t& _return, const tz_thrift::ping_t& req) override {
    _return.__set_code(0);
    _return.__set_desc("OK");
}
