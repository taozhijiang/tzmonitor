#ifndef __TZ_MONITOR_SERVICE_H__
#define __TZ_MONITOR_SERVICE_H__

#include "General.h"
#include <utils/Log.h>

#include <thrifting/gen-cpp/monitor_types.h>
#include <thrifting/gen-cpp/monitor_service.h>

#include <thrifting/helper/TThriftServer.h>

// ServerHelperType for TNonblockingHelper, TThreadedHelper, TThreadPoolHelper
// For specific problem, do not need to impl all ServiceHelperType

class TzMonitorHandler: public tz_thrift::monitor_serviceIf{
public:
    // override your business
    virtual void ping_test(tz_thrift::result_t& _return, const tz_thrift::ping_t& req) override;
    virtual void ev_submit(tz_thrift::result_t& result, const tz_thrift::ev_report_t& req) override;
};


typedef tz_thrift::monitor_serviceProcessor TzMonitorProcessor;
typedef tz_thrift::monitor_serviceClient    TzMonitorClient;

template < template <typename, typename> class ServerHelperType>
class TzMonitorService: public TThriftServer<TzMonitorHandler, TzMonitorProcessor, ServerHelperType> {

typedef TThriftServer<TzMonitorHandler, TzMonitorProcessor, ServerHelperType> TThriftServerType;
public:
    TzMonitorService(uint16_t port) :
        TThriftServerType(port) {
    }

    TzMonitorService(uint16_t port, uint16_t thread_sz) :
        TThriftServerType(port, thread_sz) {
    }

    TzMonitorService(uint16_t port, uint16_t thread_sz, uint16_t io_thread_sz) :
        TThriftServerType(port, thread_sz, io_thread_sz) {
    }

    virtual ~TzMonitorService() {}
};


#endif // __TZ_MONITOR_SERVICE_H__
