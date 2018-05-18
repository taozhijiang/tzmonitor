#ifndef _TZ_EVENT_ITEM_H__
#define _TZ_EVENT_ITEM_H__

#include "General.h"

struct event_data_t {
    std::string name;
    std::string msgid;     // 消息ID，只需要在time_identity_event 域下唯一即可
    int64_t     value;
    std::string flag;       // 标识区分，比如成功、失败、结果类别等
};

struct event_report_t {
    std::string version;    // 1.0.0

    time_t      time;       // 事件发生时间

    std::string host;       // 事件主机名或者主机IP
    std::string serv;    // 汇报服务名
    std::string entity_idx; // 汇报服务标识(多实例时候使用，否则默认1)

    std::vector<event_data_t> data;    // 事件不必相同，但是必须同一个time
};


typedef std::map<std::string, std::vector<event_data_t>> events_t;
typedef std::shared_ptr<events_t>                        events_ptr_t;
typedef std::map<time_t, std::shared_ptr<events_t>>      timed_events_t;

static inline std::string construct_identity(const std::string& host, const std::string& service, const std::string& entity_idx) {
    std::stringstream ss;
    ss << host << "_" << service << "_" << entity_idx;
    return ss.str();
}

#endif // _TZ_EVENT_ITEM_H__
