#include <string>
#include <iostream>
#include <syslog.h>


#include <Client/include/MonitorClient.h>

using namespace tzmonitor_client;

int main(int argc, char* argv[]) {

    std::string addr_ip = "127.0.0.1";
    uint16_t    addr_port = 8435;


    auto reporter = std::make_shared<MonitorClient>();
    if (!reporter || !reporter ->init(addr_ip, addr_port, ::syslog)) {
        std::cout << "init client failed." << std::endl;
        return -1;
    }

    std::cout << "ping ====================>" << std::endl;
    if (reporter->ping()) {
        std::cout << "client call ping failed." << std::endl;
        return -1;
    }

    std::cout << "report ====================>" << std::endl;
    if (reporter->report_event("metric_1", 24, "T") != 0) {
        std::cout << "client call report_event failed." << std::endl;
        return -1;
    }
    reporter->report_event("metric_1", 124, "F");
    reporter->report_event("metric_2", 100);


    std::cout << "known_services ====================>" << std::endl;
    std::vector<std::string> services;
    if (reporter->known_services(services) != 0) {
        std::cout << "client call known_services failed." << std::endl;
        return -1;
    }
    std::cout << "services info:" << std::endl;
    for (auto iter = services.cbegin(); iter != services.cend(); ++iter) {
        std::cout << *iter << ", ";
    }
    std::cout << std::endl;

    std::cout << "known_metrics ====================>" << std::endl;
    std::vector<std::string> metrics;
    if (reporter->known_metrics(metrics) != 0) {
        std::cout << "client call known_metrics failed." << std::endl;
        return -1;
    }
    std::cout << "metrics info:" << std::endl;
    for (auto iter = metrics.cbegin(); iter != metrics.cend(); ++iter) {
        std::cout << *iter << ", ";
    }
    std::cout << std::endl;


    std::cout << "select gp tag ====================>" << std::endl;
    event_select_t stat;
    if (reporter->select_stat_groupby_tag("metric_1", stat, 2*60*60) != 0) {
        std::cout << "client call select_stat_by_tag failed." << std::endl;
        return -1;
    }
    std::cout << "select_stat_by_tag info:" << std::endl;
    std::cout << "\ttimestamp:" << stat.timestamp << ", ";
    std::cout << "\ttm_interval:" << stat.tm_interval << ", ";
    std::cout << "\tservice:" << stat.service << ", ";
    std::cout << "\tmetric:" << stat.metric << ", ";
    std::cout << "\tentity_idx:" << stat.entity_idx << ", ";
    std::cout << "\ttag:" << stat.tag << ", ";
    std::cout << std::endl;

    char buffer[2048];
    snprintf(buffer, sizeof(buffer), "summray: tag:%s, count:%d, sum:%ld, avg:%ld, std:%f",
             stat.summary.tag.c_str(), stat.summary.count, stat.summary.value_sum,
             stat.summary.value_avg, stat.summary.value_std);
    std::cout << buffer << std::endl;

    for (auto iter = stat.info.begin(); iter != stat.info.end(); ++iter) {
        snprintf(buffer, sizeof(buffer), "detail=> tag:%s, count:%d, sum:%ld, avg:%ld, std:%f",
                 iter->tag.c_str(), iter->count, iter->value_sum,
                 iter->value_avg, iter->value_std);
        std::cout << buffer << std::endl;
    }

    std::cout << "select gp timestamp ====================>" << std::endl;
    event_select_t stat2;
    if (reporter->select_stat_groupby_time("metric_1", "F", stat2, 2*60*60) != 0) {
        std::cout << "client call select_stat_by_timstamp failed." << std::endl;
        return -1;
    }
    std::cout << "select_stat_by_tag info:" << std::endl;
    std::cout << "\ttimestamp:" << stat2.timestamp << ", ";
    std::cout << "\ttm_interval:" << stat2.tm_interval << ", ";
    std::cout << "\tservice:" << stat2.service << ", ";
    std::cout << "\tmetric:" << stat2.metric << ", ";
    std::cout << "\tentity_idx:" << stat2.entity_idx << ", ";
    std::cout << "\ttag:" << stat2.tag << ", ";
    std::cout << std::endl;

    snprintf(buffer, sizeof(buffer), "summray: tag:%s, count:%d, sum:%ld, avg:%ld, std:%f",
             stat2.summary.tag.c_str(), stat2.summary.count, stat2.summary.value_sum,
             stat2.summary.value_avg, stat2.summary.value_std);
    std::cout << buffer << std::endl;

    for (auto iter = stat2.info.begin(); iter != stat2.info.end(); ++iter) {
        snprintf(buffer, sizeof(buffer), "detail=> timestamp:%ld, count:%d, sum:%ld, avg:%ld, std:%f",
                 iter->timestamp, iter->count, iter->value_sum,
                 iter->value_avg, iter->value_std);
        std::cout << buffer << std::endl;
    }

    ::sleep(10);

    std::cout << "startup ok" << std::endl;
    return 0;
}
