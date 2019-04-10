/*-
 * Copyright (c) 2019 TAO Zhijiang<taozhijiang@gmail.com>
 *
 * Licensed under the BSD-3-Clause license, see LICENSE for full information.
 *
 */


#ifndef __BUSINESS_STORE_IF_H__
#define __BUSINESS_STORE_IF_H__

#include <xtra_rhel.h>

#include <libconfig.h++>
#include <Business/EventItem.h>


class StoreIf {

public:

    virtual bool init(const libconfig::Config& conf) = 0;

    // 插入事件
    virtual int insert_ev_stat(const event_insert_t& stat) = 0;

    // 查询事件
    // 因为linger会有一部分事件肯定是在途的，所以查询的时候将这部分时间优化掉
    virtual int select_ev_stat(const event_cond_t& cond, event_select_t& stat,
                               time_t linger_hint) = 0;

    // 获取所有的metrics列表
    virtual int select_metrics(const std::string& service, std::vector<std::string>& metrics) = 0;
    virtual int select_services(std::vector<std::string>& services) = 0;

};


std::shared_ptr<StoreIf> StoreFactory(const std::string& storeType);


#endif // __BUSINESS_STORE_IF_H__
