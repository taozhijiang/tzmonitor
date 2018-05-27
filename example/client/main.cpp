#include <iostream>

#include "TzMonitor.h"

int main(int arcg, char* argv[]) {

    auto client = std::make_shared<TzMonitor::TzMonitorClient>("centos", "testservice");
    if(!client->init("tzmonitor.conf")) {
        return -1;
    }

    std::cout << "initialize TzMonitor client ok " << std::endl;

    while (true) {
        client->report_event("event1", 200, "flag_T");
        client->report_event("event1", 100, "flag_F");
        client->report_event("event2", 100, "flag_T");
        client->report_event("event2", 200, "flag_F");
        client->report_event("event2", 200, "flag_F");
        client->report_event("event2", 200, "flag_F");
        client->report_event("event2", 200, "flag_F");
        ::usleep(5);
    }

    std::cout << "dummy test done!" << std::endl;

    return 0;
}

