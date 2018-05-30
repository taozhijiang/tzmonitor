#define BOOST_TEST_MODULE thrift_event_submit

#include <boost/test/unit_test.hpp>

#include "General.h"

#include <string>
#include <functional>

#include <utils/Utils.h>

#include <boost/shared_ptr.hpp>
#include <boost/make_shared.hpp>

#include <thrifting/helper/TThriftClient.h>
#include <thrifting/biz/TzMonitorService.h>

#include <core/EventItem.h>

// 类似namespace的保护
BOOST_AUTO_TEST_SUITE(thrift_event_submit)

BOOST_AUTO_TEST_CASE(thrift_event_submit)
{
    std::string config_file = "../tzmonitor.conf";
    if (!sys_config_init(config_file)) {
        BOOST_CHECK(false);
    }

    std::string serv_addr;
    int listen_port = 0;
    if (!get_config_value("thrift.serv_addr", serv_addr) || !get_config_value("thrift.listen_port", listen_port) ){
        BOOST_CHECK(false);
    }

    tz_thrift::ev_report_t req {};
    tz_thrift::result_t result {};
    std::vector<tz_thrift::ev_data_t> data {};
    tz_thrift::ev_data_t item {};
    item.name = "callsrvtime";
    item.msgid = 6001;
    item.value = 100;
    item.flag = "T";
    data.push_back(item);

    item.msgid = 6002;
    item.value = 150;
    item.flag = "T";
    data.push_back(item);

    item.msgid = 6003;
    item.value = 80;
    item.flag = "F";
    data.push_back(item);

    req.version = "1.0.0";
    req.host = "centos";
    req.serv = "testservice";
    req.entity_idx = "1";
    req.time = ::time(NULL);
    req.data = data;

    int ret = TThriftClient::call_service<TzMonitorClient>(serv_addr, static_cast<uint16_t>(listen_port),
                                                           &TzMonitorClient::ev_submit, std::ref(result), std::cref(req));

    if (ret == 0 && result.code == 0 && result.desc == "OK") {
        BOOST_CHECK(true);
        return;
    }

    BOOST_CHECK(false);
}

BOOST_AUTO_TEST_SUITE_END()
