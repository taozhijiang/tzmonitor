#include <iostream>

#include "TzMonitor.h"

int main(int arcg, char* argv[]) {

    auto client = std::make_shared<TzMonitor::TzMonitorClient>("centos", "testservice");
    if(!client->init("tzmonitor.conf")) {
        return -1;
    }

    while (true) {
        client->report_event("event1", 100, "flag_T");
        client->report_event("event1", 200, "flag_F");
        client->report_event("event2", 300, "flag_T");
        client->report_event("event2", 400, "flag_F");
        client->report_event("event3", 500, "flag_T");
        client->report_event("event3", 600, "flag_F");

        client->report_event("event1", 600, "flag_T");
        client->report_event("event1", 500, "flag_F");
        client->report_event("event2", 400, "flag_T");
        client->report_event("event2", 300, "flag_F");
        client->report_event("event3", 200, "flag_T");
        client->report_event("event3", 100, "flag_F");

        ::usleep(1);
    }

    log_info("TzMonitor client test done ");

    return 0;
}

