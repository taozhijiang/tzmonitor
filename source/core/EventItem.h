/*-
 * Copyright (c) 2018 TAO Zhijiang<taozhijiang@gmail.com>
 *
 * Licensed under the BSD-3-Clause license, see LICENSE for full information.
 *
 */


#ifndef _TZ_EVENT_ITEM_H__
#define _TZ_EVENT_ITEM_H__

// in include dir, also as API
#include "EventTypes.h"
#include <map>
#include <sstream>

// 提交插入条目
struct event_insert_t {
    std::string host;
    std::string serv;
    std::string entity_idx;
    time_t      time;

    std::string name;
    std::string flag;

    int         count;
    int64_t     value_sum;
    int64_t     value_avg;
    double      value_std;
};


struct events_t {
    time_t      time;
    std::map<std::string, std::vector<event_data_t>> data;

public:
    explicit events_t(time_t tm):
        time(tm) {}
};

typedef std::shared_ptr<events_t>       events_ptr_t;
typedef std::map<time_t, events_ptr_t>  timed_events_ptr_t;

static inline std::string construct_identity(const std::string& host, const std::string& service, const std::string& entity_idx) {
    std::stringstream ss;
    ss << host << "_" << service << "_" << entity_idx;
    return ss.str();
}

#endif // _TZ_EVENT_ITEM_H__
