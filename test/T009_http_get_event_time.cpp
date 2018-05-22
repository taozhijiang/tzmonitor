#define BOOST_TEST_MODULE http_get_event_time_

#include <boost/test/unit_test.hpp>

#include "General.h"

#include <sstream>
#include <string>
#include <iostream>

#include <json/json.h>

#include <utils/Utils.h>
#include <utils/HttpUtil.h>

#include <core/EventItem.h>

// 类似namespace的保护
BOOST_AUTO_TEST_SUITE(http_get_event_time)

BOOST_AUTO_TEST_CASE(http_get_event_time)
{
    std::string config_file = "../tzmonitor.conf";
    if (!sys_config_init(config_file)) {
        BOOST_CHECK(false);
    }

    int listen_port = 0;
    if (!get_config_value("http.listen_port", listen_port) ){
        BOOST_CHECK(false);
    }

    std::stringstream ss;
    ss << "http://127.0.0.1:" << listen_port << "/ev_query?";
    ss << "groupby=time&version=1.0.0&interval_sec=300&name=callsrvtime";
    std::string sUrl = ss.str();

    HttpUtil::HttpClient client;
    if (client.GetByHttp(sUrl)) {
        BOOST_CHECK(false);
    }

    std::string rdata = client.GetData();
    std::cerr  << "Response:" << rdata << std::endl;

    BOOST_CHECK(true);
}

BOOST_AUTO_TEST_SUITE_END()
