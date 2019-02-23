/*-
 * Copyright (c) 2018 TAO Zhijiang<taozhijiang@gmail.com>
 *
 * Licensed under the BSD-3-Clause license, see LICENSE for full information.
 *
 */


#ifndef __MONITOR_RPC_CLIENT_HELPER_H__
#define __MONITOR_RPC_CLIENT_HELPER_H__

// 产生一个编译防火墙

#include <string>
#include <vector>

#include <memory>

#include <Client/include/EventTypes.h>

namespace tzmonitor_client {

class MonitorRpcClientHelper {
public:

    MonitorRpcClientHelper(const std::string& ip, uint16_t port);
    ~MonitorRpcClientHelper();

    int rpc_ping();

    int rpc_event_submit(const event_report_t& report);
    int rpc_event_select(const event_cond_t& cond, event_select_t& resp_info);
    int rpc_known_metrics(const metrics_cond_t& cond, metrics_t& metric);

private:
    class Impl;
    std::unique_ptr<Impl> impl_ptr_;
};

} // end namespace tzmonitor_client

#endif // __MONITOR_RPC_CLIENT_HELPER_H__
