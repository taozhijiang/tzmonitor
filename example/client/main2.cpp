#include <iostream>
#include <syslog.h>

#include "TzMonitor.h"

int main(int arcg, char* argv[]) {

    ::openlog(program_invocation_short_name, LOG_PID , LOG_LOCAL6);
    ::setlogmask (LOG_UPTO (7));

    auto client = std::make_shared<TzMonitor::TzMonitorClient>("centos", "testservice2");
    if(!client->init("tzmonitor_client.conf", syslog)) {
        return -1;
    }

    syslog(LOG_NOTICE, "TzMonitor client test start");

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

    syslog(LOG_NOTICE, "TzMonitor client test done");

    return 0;
}

