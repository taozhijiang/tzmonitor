#define BOOST_TEST_MODULE http_event_submit_helper

#include <boost/test/unit_test.hpp>

#include "General.h"

#include <string>
#include <functional>

#include <utils/Utils.h>

#include <boost/shared_ptr.hpp>
#include <boost/make_shared.hpp>

#include <client/TzMonitorHttpClientHelper.h>
#include <client/include/EventTypes.h>

// 类似namespace的保护
BOOST_AUTO_TEST_SUITE(http_event_submit_helper)

BOOST_AUTO_TEST_CASE(http_event_submit_helper)
{
    std::string config_file = "../tzmonitor.conf";
    if (!sys_config_init(config_file)) {
        BOOST_CHECK(false);
    }

    std::string serv_addr;
    int listen_port = 0;
    if (!get_config_value("http.serv_addr", serv_addr) || !get_config_value("http.listen_port", listen_port) ){
        BOOST_CHECK(false);
    }

    std::stringstream ss;
    ss << "http://" << serv_addr << ":" << listen_port << "/";
    std::string sUrl = ss.str();


    event_report_t report {};
    report.version = "1.0.0";
    report.host = "centos";
    report.serv = "testservice";
    report.entity_idx = "1";
    report.time = ::time(NULL);

    std::vector<event_data_t> data = {
        {"callevent2", 15001, 20, "3T" },
        {"callevent2", 15002, 800, "3T" },
        {"callevent1", 15003, 50, "3F" },
    };
    report.data = data;

    TzMonitorHttpClientHelper client(sUrl);
    int ret = client.http_event_submit(report);

    if (ret == 0) {
        BOOST_CHECK(true);
    } else {
        BOOST_CHECK(false);
    }
}

BOOST_AUTO_TEST_SUITE_END()
