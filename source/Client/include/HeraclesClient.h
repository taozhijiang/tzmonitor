/*-
 * Copyright (c) 2019 TAO Zhijiang<taozhijiang@gmail.com>
 *
 * Licensed under the BSD-3-Clause license, see LICENSE for full information.
 *
 */


#ifndef __MONITOR_CLIENT_H__
#define __MONITOR_CLIENT_H__

#include <errno.h>
#include <libconfig.h++>

#include <memory>

#include "EventTypes.h"

typedef void (* CP_log_store_func_t)(int priority, const char* format, ...);

// 可以创建多个HeraclesClient的客户端，但是后台实现只会使用
// 一个单例来实现，否则产生的msgid会重复，导致消息被丢弃

// 为什么不直接使用单例？单例用起来是在太臭了
namespace heracles_client {

struct order_cond_t {
    enum OrderByType orderby_;
    enum OrderType   orders_;
    int32_t          limit_;   // 限制返回排序后记录的条数

    order_cond_t(enum OrderByType btp, enum OrderType tp = OrderType::kOrderDesc, int32_t limit = 0) :
        orderby_(btp),
        orders_(tp),
        limit_(limit) {
    }
};

class HeraclesClient : public std::enable_shared_from_this<HeraclesClient> {
public:
    explicit HeraclesClient(std::string entity_idx = "");
    HeraclesClient(std::string service, std::string entity_idx = "");

    ~HeraclesClient();

    // 禁止拷贝
    HeraclesClient(const HeraclesClient&) = delete;
    HeraclesClient& operator=(const HeraclesClient&) = delete;

    // 用存量的cfg进行更新，必须确保cfgFile_已经初始化了
    bool init();
    bool init(const std::string& cfgFile, CP_log_store_func_t log_func);
    bool init(const libconfig::Setting& setting, CP_log_store_func_t log_func);
    bool init(const std::string& addr, uint16_t port, CP_log_store_func_t log_func);  // 简易，使用默认参数
    bool init(const std::string& service, const std::string& entity_idx,
              const std::string& addr, uint16_t port,
              CP_log_store_func_t log_func);

    int ping();

    int report_event(const std::string& metric, int64_t value, std::string tag = "T");

    // 常用便捷接口
    int select_stat(const std::string& metric, int64_t& count, int64_t& avg, time_t tm_intervel = 60);
    int select_stat(const std::string& metric, const std::string& tag, int64_t& count, int64_t& avg, time_t tm_intervel = 60);

    int select_stat_groupby_tag(const std::string& metric, event_select_t& stat, time_t tm_intervel = 60);
    int select_stat_groupby_time(const std::string& metric, event_select_t& stat, time_t tm_intervel = 60);
    int select_stat_groupby_time(const std::string& metric, const std::string& tag, event_select_t& stat, time_t tm_intervel = 60);

    int select_stat_groupby_tag_ordered(const std::string& metric, const order_cond_t& order, event_select_t& stat, time_t tm_intervel = 60);
    int select_stat_groupby_time_ordered(const std::string& metric, const order_cond_t& order, event_select_t& stat, time_t tm_intervel = 60);
    int select_stat_groupby_time_ordered(const std::string& metric, const std::string& tag,
                                         const order_cond_t& order, event_select_t& stat, time_t tm_intervel = 60);

    // 最底层的接口，可以做更加精细化的查询
    int select_stat(event_cond_t& cond, event_select_t& stat);

    // 查询所有已经上报的metrics, service == ""就默认是自己的service
    int known_metrics(event_handler_conf_t& handler_conf, std::vector<std::string>& metrics, std::string service = "");
    int known_services(std::vector<std::string>& services);

    // 通用扩展接口
    int module_runtime(const libconfig::Config& conf);
    int module_status(std::string& module, std::string& name, std::string& val);
};

} // end namespace heracles_client

#endif // __MONITOR_CLIENT_H__
