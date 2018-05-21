#define BOOST_TEST_MODULE thrift_event_query_detail

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
BOOST_AUTO_TEST_SUITE(thrift_event_query_detail)

BOOST_AUTO_TEST_CASE(thrift_event_query_detail)
{
    std::string config_file = "../tzmonitor.conf";
    if (!sys_config_init(config_file)) {
        BOOST_CHECK(false);
    }

    int listen_port = 0;
    if (!get_config_value("thrift.listen_port", listen_port) ){
        BOOST_CHECK(false);
    }

    tz_thrift::ev_query_request_t req {};
    req.version = "1.0.0";
    req.name = "callsrvtime";
    req.interval_sec = 600;

    tz_thrift::ev_query_response_detail_t resp {};
    int ret = TThriftClient::call_service<TzMonitorClient>("127.0.0.1", static_cast<uint16_t>(listen_port),
                                                           &TzMonitorClient::ev_query_detail, std::ref(resp), std::cref(req));

    if (ret == 0 && resp.result.code == 0 && resp.result.desc == "OK") {
        std::cerr << "real_start: " << resp.time << std::endl;
        for (auto iter = resp.info.begin(); iter != resp.info.end(); ++iter) {
            std::cerr << "time: "     << iter->time;
            std::cerr << " count: "     << iter->count;
            std::cerr << " value_sum: " << iter->value_sum;
            std::cerr << " value_avg: " << iter->value_avg;
            std::cerr << " value_std: " << iter->value_std;
            std::cerr << std::endl;
        }
        BOOST_CHECK(true);
        return;
    }

    BOOST_CHECK(false);
}

BOOST_AUTO_TEST_SUITE_END()
