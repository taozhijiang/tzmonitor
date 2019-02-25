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
    int select_ev_stat(const event_cond_t& cond, event_select_t& stat, time_t linger_hint);

    int select_metrics(const std::string& service, std::vector<std::string>& metrics);
    int select_services(std::vector<std::string>& services);

private:
    int insert_ev_stat(tzrpc::sql_conn_ptr &conn, const event_insert_t& stat);
    int select_ev_stat(tzrpc::sql_conn_ptr& conn, const event_cond_t& cond, event_select_t& stat,
                       time_t linger_hint);

    std::string get_table_suffix(time_t time_sec);
    // 自动创建分表
    int create_table(tzrpc::sql_conn_ptr& conn,
                     const std::string& database, const std::string& prefix,
                     const std::string& service, const std::string& suffix);
    std::string build_sql(const event_cond_t& cond, time_t linger_hint, time_t& start_time);

    // 数据库连接
    std::shared_ptr<tzrpc::ConnPool<tzrpc::SqlConn, tzrpc::SqlConnPoolHelper>> sql_pool_ptr_;

    std::string database_;
    std::string table_prefix_;
};

#endif // __BUSINESS_STORE_SQL_H__
