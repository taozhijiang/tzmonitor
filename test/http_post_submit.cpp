#define BOOST_TEST_MODULE http_post_event_submit

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
BOOST_AUTO_TEST_SUITE(http_post_event_submit)

BOOST_AUTO_TEST_CASE(http_post_event_submit)
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
    ss << "http://127.0.0.1:" << listen_port << "/ev_submit";

    std::string sUrl = ss.str();

    std::map<std::string, std::string> map_param;
    map_param["version"] = "1.0.0";
    map_param["host"] = "centos";
    map_param["serv"] = "tstsrv";
    map_param["entity_idx"] = "1";
    map_param["time"] = convert_to_string(::time(NULL));


    Json::Value ordersJson;
    Json::FastWriter fast_writer;

    // for
    Json::Value ordersjson;
    ordersjson["name"] = "callsrvtime";
    ordersjson["msgid"] = "123";
    ordersjson["value"] = "22";
    ordersjson["flag"] = "f";
    ordersJson.append(ordersjson);

    ordersjson["msgid"] = "124";
    ordersjson["value"] = "33";
    ordersjson["flag"] = "f";
    ordersJson.append(ordersjson);

    ordersjson["msgid"] = "125";
    ordersjson["value"] = "10";
    ordersjson["flag"] = "t";
    ordersJson.append(ordersjson);

    ordersjson["msgid"] = "124";
    ordersjson["value"] = "77";
    ordersjson["flag"] = "f";
    ordersJson.append(ordersjson);
    // end for

    map_param["data"] = fast_writer.write(ordersJson);

    // build request json
    Json::Value root;
    for (auto itr = map_param.begin(); itr != map_param.end(); itr++) {
        root[itr->first] = itr->second;
    }
    std::string postData = fast_writer.write(root);

    HttpUtil::HttpClient client;
    if (client.PostByHttp(sUrl, postData)) {
        BOOST_CHECK(false);
    }

    std::string rdata = client.GetData();
    std::cerr  << "Response:" << rdata << std::endl;

    BOOST_CHECK(true);
}

BOOST_AUTO_TEST_SUITE_END()
