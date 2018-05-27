#ifndef __TZ_MONITOR_HTTP_CLIENT_HELPER_H__
#define __TZ_MONITOR_HTTP_CLIENT_HELPER_H__

#include <string>
#include <vector>

#include <memory>

#include "EventTypes.h"

class TzMonitorHttpClientHelper {
public:

    // append ev_query ev_submit
    explicit TzMonitorHttpClientHelper(const std::string& service_url);
    ~TzMonitorHttpClientHelper();

    int http_event_submit(const event_report_t& report);
    int http_event_query(const event_cond_t& cond, event_query_t& resp_info);

private:
    class Impl;
    std::unique_ptr<Impl> impl_ptr_;
};

#endif //  __TZ_MONITOR_HTTP_CLIENT_HELPER_H__
