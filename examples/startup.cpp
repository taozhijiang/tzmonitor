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

    if (reporter->ping()) {
        std::cout << "client call ping failed." << std::endl;
        return -1;
    }

    if (reporter->report_event("metric_1", 24, "T") != 0) {
        std::cout << "client call report_event failed." << std::endl;
        return -1;
    }
    reporter->report_event("metric_1", 24, "F");
    reporter->report_event("metric_2", 100);

    ::sleep(10);

    std::map<std::string, std::string> metrics;
    if (reporter->known_metrics(metrics) != 0) {
        std::cout << "client call known_metrics failed." << std::endl;
        return -1;
    }

    std::cout << "ping ok" << std::endl;
    return 0;
}
