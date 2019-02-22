/*-
 * Copyright (c) 2018 TAO Zhijiang<taozhijiang@gmail.com>
 *
 * Licensed under the BSD-3-Clause license, see LICENSE for full information.
 *
 */

#include <sstream>

#include <Utils/Log.h>
#include <Utils/StrUtil.h>

#include <Business/StoreSql.h>

using namespace tzrpc;

bool StoreSql::init(const libconfig::Config& conf) override {


    std::string mysql_hostname;
    int mysql_port;
    std::string mysql_username;
    std::string mysql_passwd;
    std::string mysql_database;
    if (!ConfUtil::conf_value(conf, "rpc_business.mysql.host_addr", mysql_hostname) ||
        !ConfUtil::conf_value(conf, "rpc_business.mysql.host_port", mysql_port) ||
        !ConfUtil::conf_value(conf, "rpc_business.mysql.username", mysql_username) ||
        !ConfUtil::conf_value(conf, "rpc_business.mysql.passwd", mysql_passwd) ||
        !ConfUtil::conf_value(conf, "rpc_business.mysql.database", mysql_database) ||
        !ConfUtil::conf_value(conf, "rpc_business.mysql.table_prefix", table_prefix_) )
    {
        log_err("Error, get mysql config value error");
        return false;
    }

    database_ = mysql_database;

    int conn_pool_size = 0;
    if (!ConfUtil::conf_value(conf, "rpc_business.mysql.conn_pool_size", conn_pool_size)) {
        conn_pool_size = 20;
        log_info("Using default conn_pool size: 20");
    }

//    mysql_passwd = Security::DecryptSelfParam(mysql_passwd.c_str());
    SqlConnPoolHelper helper(mysql_hostname, mysql_port,
                             mysql_username, mysql_passwd, mysql_database);
    sql_pool_ptr_.reset(new ConnPool<SqlConn, SqlConnPoolHelper>("MySQLPool", conn_pool_size, helper));
    if (!sql_pool_ptr_ || !sql_pool_ptr_->init()) {
        log_err("Init SqlConnPool failed!");
        return false;
    }


    return true;
}

std::string StoreSql::get_table_suffix(time_t time_sec) {
    struct tm now_time;
    localtime_r(&time_sec, &now_time);

    char buff[20] = {0, };
    sprintf(buff, "%04d%02d", now_time.tm_year + 1900, now_time.tm_mon + 1);

    return buff;
}

int StoreSql::insert_ev_stat(const event_insert_t& stat) override {

    sql_conn_ptr conn;
    sql_pool_ptr_->request_scoped_conn(conn);
    if (!conn) {
        log_err("request sql conn failed!");
        return -1;
    }

    return insert_ev_stat(conn, stat);
}


int StoreSql::insert_ev_stat(sql_conn_ptr& conn, const event_insert_t& stat) {

    if (!conn) {
        log_err("request sql conn failed!");
        return -1;
    }

    if (stat.service.empty() || stat.metric.empty() || stat.timestamp == 0) {
        log_err("error check error!");
        return -1;
    }

    std::string entity_idx = stat.entity_idx;
    if (entity_idx.empty()) {
        entity_idx = "1";
    }

    std::string tag = stat.tag;
    if (tag.empty()) {
        tag = "T";
    }

    std::string sql = va_format(
                   " INSERT INTO %s.%sevent_stat_%s "
                   " SET F_service = '%s', F_entity_idx = '%s', F_timestamp = %ld, "
                   " F_metric = '%s', F_tag = '%s', "
                   " F_count = %d, F_value_sum = %ld, F_value_avg = %ld, F_value_std = %f; ",
                   database_.c_str(), table_prefix_.c_str(), get_table_suffix(stat.timestamp).c_str(),
                   stat.service.c_str(), entity_idx.c_str(), stat.timestamp,
                   stat.metric.c_str(), tag.c_str(),
                   stat.count, stat.value_sum, stat.value_avg, stat.value_std);

    int nAffected = conn->sqlconn_execute_update(sql);
    return nAffected == 1 ? 0 : -1;
}


std::string StoreSql::build_sql(const event_cond_t& cond, time_t& start_time) {

    std::stringstream ss;

    time_t tm_start = ::time(NULL);
    if (cond.tm_start > 0) {
//        cond.tm_start = ::time(NULL) - EventRepos::instance().get_event_linger();
        tm_start = cond.tm_start;
    }

    if (cond.groupby == GroupType::kGroupbyTimestamp) {
        ss << "SELECT IFNULL(SUM(F_count), 0), IFNULL(SUM(F_value_sum), 0), IFNULL(AVG(F_value_std), 0), F_timestamp FROM ";
    } else if (cond.groupby == GroupType::kGroupbyTag) {
        ss << "SELECT IFNULL(SUM(F_count), 0), IFNULL(SUM(F_value_sum), 0), IFNULL(AVG(F_value_std), 0), F_tag FROM ";
    } else {
        ss << "SELECT IFNULL(SUM(F_count), 0), IFNULL(SUM(F_value_sum), 0), IFNULL(AVG(F_value_std), 0) FROM ";
    }

    ss << database_ << "." << table_prefix_ << "event_stat_" << get_table_suffix(tm_start) ;
    ss << " WHERE F_timestamp <= " << cond.tm_start <<" AND F_timestamp > " << tm_start - cond.tm_interval;
    ss << " AND F_metric = '" << cond.metric << "'";

    if (!cond.service.empty()) {
        ss << " AND F_service = '" << cond.service << "'";
    }

    if (!cond.entity_idx.empty()) {
        ss << " AND F_entity_idx = '" << cond.entity_idx << "'";
    }

    if (!cond.tag.empty()) {
        ss << " AND F_tag = '" << cond.tag << "'";
    }

    if (cond.groupby == GroupType::kGroupbyTimestamp) {
        ss << " GROUP BY F_timestamp ORDER BY F_timestamp DESC; ";
    } else if (cond.groupby == GroupType::kGroupbyTag) {
        ss << " GROUP BY F_tag; ";
    }

    std::string sql = ss.str();
    log_debug("built query str: %s", sql.c_str());

    return sql;
}


// group summary
int StoreSql::select_ev_stat(const event_cond_t& cond, event_select_t& stat) override {
    sql_conn_ptr conn;
    sql_pool_ptr_->request_scoped_conn(conn);
    if (!conn) {
        log_err("request sql conn failed!");
        return -1;
    }

    return select_ev_stat(conn, cond, stat);
}

int StoreSql::select_ev_stat(sql_conn_ptr& conn, const event_cond_t& cond, event_select_t& stat) {

    if (!conn) {
        log_err("request sql conn failed!");
        return -1;
    }

    time_t real_start_time = 0;
    std::string sql = build_sql(cond, real_start_time);
    stat.timestamp = real_start_time;

    shared_result_ptr result;
    result.reset(conn->sqlconn_execute_query(sql));
    if (!result) {
        log_err("Failed to query info: %s", sql.c_str());
        return -1;
    }

    if (result->rowsCount() == 0) {
        log_info("Empty record found!");
        return 0;
    }

      // 可能会有某个时刻没有数据的情况，这留给客户端去填充
      // 服务端不进行填充，减少网络数据的传输
    while (result->next()) {

        event_info_t item {};

        bool success = false;
        if (cond.groupby == GroupType::kGroupbyTimestamp) {
            success = cast_raw_value(result, 1, item.count, item.value_sum, item.value_std, item.timestamp);
        } else if (cond.groupby == GroupType::kGroupbyTag) {
            success = cast_raw_value(result, 1, item.count, item.value_sum, item.value_std, item.tag);
        } else {
            success = cast_raw_value(result, 1, item.count, item.value_sum, item.value_std);
        }

        if (!success) {
            log_err("failed to cast info, skip this ..." );
            continue;
        }

        if (item.count != 0) {
            item.value_avg = item.value_sum / item.count;
        } else {
            item.value_avg = 0;
        }

        stat.summary.count += item.count;
        stat.summary.value_sum += item.value_sum;
        stat.summary.value_std += item.value_std * item.count;

        stat.info.push_back(item);
    }

    if (stat.summary.count != 0) {
        stat.summary.value_avg = stat.summary.value_sum / stat.summary.count;
        stat.summary.value_std = stat.summary.value_std / stat.summary.count;   // not very well
    }

    return 0;
}

int StoreSql::select_ev_metrics(std::map<std::string, service_metric_t>& metrics) override {

    sql_conn_ptr conn;
    sql_pool_ptr_->request_scoped_conn(conn);
    if (!conn) {
        log_err("request sql conn failed!");
        return -1;
    }

#if 0

//    time_t time = ::time(NULL) - EventRepos::instance().get_event_linger();
    time_t time = ::time(NULL);

    std::stringstream ss;
    ss << "SELECT DISTINCT F_metric FROM " ;
    ss << database_ << "." << table_prefix_ << "event_stat_" << get_table_suffix(time) ;
    std::string sql = ss.str();

    if(!conn->sqlconn_execute_query_multi(ss.str(), metrics)) {
        log_err("Failed to query info: %s", sql.c_str());
        return -1;
    }
#endif

    return 0;
}
