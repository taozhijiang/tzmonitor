/*-
 * Copyright (c) 2018 TAO Zhijiang<taozhijiang@gmail.com>
 *
 * Licensed under the BSD-3-Clause license, see LICENSE for full information.
 *
 */


#ifndef __BUSINESS_EVENT_ITEM_H__
#define __BUSINESS_EVENT_ITEM_H__

// in include dir, also as API
#include <map>
#include <sstream>

#include <Business/EventTypes.h>

// 提交插入条目，数据规整后的结果，直接和数据库交互
struct event_insert_t {

    // 索引使用
    std::string service;

    std::string entity_idx;
    time_t      timestamp;

    std::string metric;
    std::string tag;

    int         count;
    int64_t     value_sum;
    int64_t     value_avg;
    double      value_std;
};

struct service_metric_t {
    std::string metric;
    std::string tag;
};

// 相同timestamp内的事件汇聚
struct events_t {
public:
    explicit events_t(time_t tm):
        timestamp_(tm) {}

    time_t      timestamp_;
    std::map<std::string, std::vector<event_data_t>> data_;
};

typedef std::shared_ptr<events_t>       events_ptr_t;
typedef std::map<time_t, events_ptr_t>  timed_events_ptr_t;

#endif // __BUSINESS_EVENT_ITEM_H__
