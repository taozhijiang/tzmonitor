#define BOOST_TEST_MODULE http_event_query_flag_helper

#include <boost/test/unit_test.hpp>

#include "General.h"

#include <string>
#include <functional>

#include <utils/Utils.h>

#include <boost/shared_ptr.hpp>
#include <boost/make_shared.hpp>

#include <client/include/TzMonitorHttpClientHelper.h>
#include <client/include/EventTypes.h>

// 类似namespace的保护
BOOST_AUTO_TEST_SUITE(http_event_query_flag_helper)

BOOST_AUTO_TEST_CASE(http_event_query_flag_helper)
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

    event_cond_t cond {};
    cond.version = "1.0.0";
    cond.name = "callevent3";
    cond.interval_sec = 300;
    cond.groupby = GroupType::kGroupbyFlag;

    event_query_t result {};
    TzMonitorHttpClientHelper client(sUrl);
    int ret = client.http_event_query(cond, result);

    if (ret == 0) {
        std::cerr << "real_start: " << result.time << std::endl;

        std::cerr << "summray> :";
        std::cerr << " count: "     << result.summary.count;
        std::cerr << " value_sum: " << result.summary.value_sum;
        std::cerr << " value_avg: " << result.summary.value_avg;
        std::cerr << " value_std: " << result.summary.value_std;
        std::cerr << std::endl;

        int i = 0;
        for (auto iter = result.info.begin(); iter != result.info.end(); ++iter) {
            std::cerr << i++ ;
            std::cerr << "> flag: "       << iter->flag;
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
