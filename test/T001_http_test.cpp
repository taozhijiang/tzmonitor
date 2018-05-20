#define BOOST_TEST_MODULE http_test

#include <boost/test/unit_test.hpp>

#include "General.h"

#include <sstream>
#include <string>
#include <iostream>

#include <utils/Utils.h>
#include <utils/HttpUtil.h>

// 类似namespace的保护
BOOST_AUTO_TEST_SUITE(http_test)

BOOST_AUTO_TEST_CASE(httpget)
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
    ss << "http://127.0.0.1:" << listen_port << "/test";

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
