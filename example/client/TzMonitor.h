#ifndef _TZ_MONITOR_CLIENT_H__
#define _TZ_MONITOR_CLIENT_H__

#include <memory>
#include <boost/noncopyable.hpp>

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

private:
    class Impl;
    std::shared_ptr<Impl> impl_ptr_;
};

} // end namespace

#endif // _TZ_MONITOR_CLIENT_H__
