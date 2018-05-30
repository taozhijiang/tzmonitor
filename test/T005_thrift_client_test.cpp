#define BOOST_TEST_MODULE thrift_client_test

#include <boost/test/unit_test.hpp>

#include <iostream>
#include <string>
#include <functional>

#include <utils/Utils.h>

#include <boost/shared_ptr.hpp>
#include <boost/make_shared.hpp>

#include <thrifting/helper/TThriftClient.h>
#include <thrifting/biz/TzMonitorService.h>

BOOST_AUTO_TEST_SUITE(thrift_client_test)

BOOST_AUTO_TEST_CASE(pingA)
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

    tz_thrift::ping_t req;
    req.__set_msg("TTTZZZ");

    tz_thrift::result_t result;
    int ret = TThriftClient::call_service<TzMonitorClient>(serv_addr, static_cast<uint16_t>(listen_port),
                                                           &TzMonitorClient::ping_test, std::ref(result), std::cref(req));

    if (ret == 0 && result.code == 0 && result.desc == "OK") {
        std::cerr << "server return OK" << std::endl;
        BOOST_CHECK(true);
        return;
    }

    BOOST_CHECK(false);
}


// Create Client & Call Service
BOOST_AUTO_TEST_CASE(pingB)
{
    std::string config_file = "../tzmonitor.conf";
    if (!sys_config_init(config_file)) {
        BOOST_CHECK(false);
    }

    int listen_port = 0;
    if (!get_config_value("thrift.listen_port", listen_port) ){
        BOOST_CHECK(false);
    }

//    boost::optional<std::shared_ptr<TzMonitorClient>> client
    auto client
                = TThriftClient::create_client<TzMonitorClient>("127.0.0.1", static_cast<uint16_t>(listen_port));

    BOOST_CHECK(client);

    tz_thrift::ping_t req;
    req.__set_msg("TTT222");

    tz_thrift::result_t result;
    int ret = TThriftClient::call_service<TzMonitorClient>(*client, &TzMonitorClient::ping_test, std::ref(result), std::cref(req));

    if (ret == 0 && result.code == 0 && result.desc == "OK") {
        std::cerr << "server return OK" << std::endl;
        BOOST_CHECK(true);
        return;
    }

    BOOST_CHECK(false);
}

BOOST_AUTO_TEST_SUITE_END()
