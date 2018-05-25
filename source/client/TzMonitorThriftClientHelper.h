#ifndef __TZ_MONITOR_THRIFT_CLIENT_HELPER_H__
#define __TZ_MONITOR_THRIFT_CLIENT_HELPER_H__

// 产生一个编译防火墙，让客户端不用依赖于Thrift的各种头文件，只需要单独的头文件和库连接就可以了

#include <string>
#include <vector>

#include <memory>

#include <EventTypes.h>


class TzMonitorThriftClientHelper {
public:

    TzMonitorThriftClientHelper(const std::string& ip, uint16_t port);
    ~TzMonitorThriftClientHelper();

    int thrift_event_submit(const event_report_t& report);
    int thrift_event_query(const event_cond_t& cond, event_query_t& resp_info);

private:
    class Impl;
    std::unique_ptr<Impl> impl_ptr_;
};

#endif // __TZ_MONITOR_THRIFT_CLIENT_HELPER_H__
