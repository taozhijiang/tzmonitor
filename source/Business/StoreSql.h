/*-
 * Copyright (c) 2018 TAO Zhijiang<taozhijiang@gmail.com>
 *
 * Licensed under the BSD-3-Clause license, see LICENSE for full information.
 *
 */


#ifndef __BUSINESS_STORE_SQL_H__
#define __BUSINESS_STORE_SQL_H__

#include <Connect/SqlConn.h>

#include <Business/StoreIf.h>

class StoreSql: public StoreIf {
public:

    bool init(const libconfig::Config& conf);
    int insert_ev_stat(const event_insert_t& stat);
    int select_ev_stat(const event_cond_t& cond, event_select_t& stat);
    int select_ev_metrics(std::map<std::string, service_metric_t>& metrics);

private:
    int insert_ev_stat(tzrpc::sql_conn_ptr &conn, const event_insert_t& stat);
    int select_ev_stat(tzrpc::sql_conn_ptr& conn, const event_cond_t& cond, event_select_t& stat);

    std::string get_table_suffix(time_t time_sec);
    std::string build_sql(const event_cond_t& cond, time_t& start_time);

    // 数据库连接
    std::shared_ptr<tzrpc::ConnPool<tzrpc::SqlConn, tzrpc::SqlConnPoolHelper>> sql_pool_ptr_;

    std::string database_;
    std::string table_prefix_;
};

#endif // __BUSINESS_STORE_SQL_H__
