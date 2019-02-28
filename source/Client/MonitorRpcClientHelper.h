/*-
 * Copyright (c) 2018 TAO Zhijiang<taozhijiang@gmail.com>
 *
 * Licensed under the BSD-3-Clause license, see LICENSE for full information.
 *
 */


#ifndef __MONITOR_RPC_CLIENT_HELPER_H__
#define __MONITOR_RPC_CLIENT_HELPER_H__

#include <string>
#include <vector>

#include <memory>


#include <boost/noncopyable.hpp>

#include <Client/include/EventTypes.h>

namespace tzmonitor_client {

class MonitorRpcClientHelper: private boost::noncopyable {
public:

    MonitorRpcClientHelper(const std::string& ip, uint16_t port):
        ip_(ip),
        port_(port),
        rpc_client_() {
    }

    ~MonitorRpcClientHelper() {
    }

    int rpc_ping();

    int rpc_event_submit(const event_report_t& report);
    int rpc_event_select(const event_cond_t& cond, event_select_t& resp_info);

    int rpc_known_metrics(const std::string& version, const std::string& service,
                          event_handler_conf_t& handler_conf, std::vector<std::string>& metrics);
    int rpc_known_services(const std::string& version, std::vector<std::string>& services);

private:
    std::string ip_;
    uint16_t    port_;

    std::unique_ptr<RpcClient> rpc_client_;
};

} // end namespace tzmonitor_client

#endif // __MONITOR_RPC_CLIENT_HELPER_H__
