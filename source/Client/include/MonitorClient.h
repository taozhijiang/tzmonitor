/*-
 * Copyright (c) 2018 TAO Zhijiang<taozhijiang@gmail.com>
 *
 * Licensed under the BSD-3-Clause license, see LICENSE for full information.
 *
 */


#ifndef __MONITOR_CLIENT_H__
#define __MONITOR_CLIENT_H__

#include <errno.h>
#include <libconfig.h++>

#include <memory>
#include <boost/noncopyable.hpp>

#include "EventTypes.h"

typedef void(* CP_log_store_func_t)(int priority, const char *format, ...);

namespace tzmonitor_client {

class MonitorClient: public boost::noncopyable {
public:
    explicit MonitorClient(std::string entity_idx = "");
    MonitorClient(std::string service, std::string entity_idx = "");

    ~MonitorClient();

    bool init(const std::string& cfgFile, CP_log_store_func_t log_func);
    bool init(const libconfig::Setting& setting, CP_log_store_func_t log_func);
    bool init(const std::string& addr,  uint16_t port, CP_log_store_func_t log_func);  // 简易，使用默认参数

    int ping();

    int report_event(const std::string& metric, int64_t value, std::string tag = "T");

    // 常用便捷接口
    int select_stat(const std::string& metric, int64_t& count, int64_t& avg, time_t tm_intervel = 60);
    int select_stat(const std::string& metric, const std::string& tag, int64_t& count, int64_t& avg, time_t tm_intervel = 60);

    int select_stat_groupby_tag (const std::string& metric, event_select_t& stat, time_t tm_intervel = 60);
    int select_stat_groupby_time(const std::string& metric, event_select_t& stat, time_t tm_intervel = 60);
    int select_stat_groupby_time(const std::string& metric, const std::string& tag, event_select_t& stat, time_t tm_intervel = 60);

    // 最底层的接口，可以做更加精细化的查询
    int select_stat(event_cond_t& cond, event_select_t& stat);

    // 查询所有已经上报的metrics, service == ""就默认是自己的service
    int known_metrics(std::vector<std::string>& metrics, std::string service = "");
    int known_services(std::vector<std::string>& services);

private:
    class Impl;
    std::shared_ptr<Impl> impl_ptr_;
};

} // end namespace tzmonitor_client

#endif // __MONITOR_CLIENT_H__
