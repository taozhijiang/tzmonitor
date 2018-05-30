#ifndef _TZ_MONITOR_CLIENT_H__
#define _TZ_MONITOR_CLIENT_H__

#include <memory>
#include <boost/noncopyable.hpp>
#include "EventTypes.h"

#ifndef _DEFINE_GET_POINTER_MARKER_
#define _DEFINE_GET_POINTER_MARKER_
template<class T>
T * get_pointer(std::shared_ptr<T> const& p) {
    return p.get();
}
#endif // _DEFINE_GET_POINTER_MARKER_

namespace TzMonitor {

class TzMonitorClient: public boost::noncopyable {
public:
    TzMonitorClient(std::string host, std::string serv, std::string entity_idx = "1");
    ~TzMonitorClient();
    bool init(const std::string& cfgFile);

    int report_event(const std::string& name, int64_t value, std::string flag = "T");

    // 常用便捷接口
    int retrieve_stat(const std::string& name, int64_t& count, int64_t& avg, time_t intervel_sec = 60);
    int retrieve_stat(const std::string& name, const std::string& flag, int64_t& count, int64_t& avg, time_t intervel_sec = 60);

    int retrieve_stat_flag(const std::string& name, event_query_t& stat, time_t intervel_sec = 60);
    int retrieve_stat_time(const std::string& name, event_query_t& stat, time_t intervel_sec = 60);
    int retrieve_stat_time(const std::string& name, const std::string& flag, event_query_t& stat, time_t intervel_sec = 60);

    // 最底层的接口，可以做更加精细化的查询
    int retrieve_stat(const event_cond_t& cond, event_query_t& stat);

private:
    class Impl;
    std::shared_ptr<Impl> impl_ptr_;
};

} // end namespace

#endif // _TZ_MONITOR_CLIENT_H__
