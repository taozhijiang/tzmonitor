#define BOOST_TEST_MODULE client_query_test
#include <boost/test/included/unit_test.hpp>

#include <iostream>
#include <client/include/TzMonitor.h>
#include <client/include/EventTypes.h>

// 类似namespace的保护
BOOST_AUTO_TEST_SUITE(client_query_test)

BOOST_AUTO_TEST_CASE(client_query_test)
{
    auto client = std::make_shared<TzMonitor::TzMonitorClient>("centos", "testservice");
    if(!client->init("../tzmonitor.conf")) {
        BOOST_CHECK(false);
    }

    int64_t count = 0;
    int64_t avg = 0;

    BOOST_CHECK (client->retrieve_stat("event1", count, avg) == 0);
    std::cerr << "event_unit, count:" << count << ", avg:" << avg << std::endl;

    BOOST_CHECK (client->retrieve_stat("event1", "flag_T", count, avg) == 0);
    std::cerr << "event_unit & flag_T, count:" << count << ", avg:" << avg << std::endl;

    BOOST_CHECK(true);
}

BOOST_AUTO_TEST_SUITE_END()
