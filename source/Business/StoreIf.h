/*-
 * Copyright (c) 2018 TAO Zhijiang<taozhijiang@gmail.com>
 *
 * Licensed under the BSD-3-Clause license, see LICENSE for full information.
 *
 */


#ifndef __BUSINESS_STORE_IF_H__
#define __BUSINESS_STORE_IF_H__

#include <libconfig.h++>
#include <Business/EventItem.h>


class StoreIf {

public:

    virtual bool init(const libconfig::Config& conf) = 0;

    // 插入事件
    virtual int insert_ev_stat(const event_insert_t& stat) = 0;

    // 查询事件
    virtual int select_ev_stat(const event_cond_t& cond, event_select_t& stat) = 0;

    // 获取所有的metrics列表
    virtual int select_ev_metrics(std::map<std::string, service_metric_t>& metrics) = 0;

};


#endif // __BUSINESS_STORE_IF_H__
