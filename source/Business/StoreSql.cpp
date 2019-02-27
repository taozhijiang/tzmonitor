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

// 自动创建分表
int StoreSql::create_table(sql_conn_ptr& conn,
                           const std::string& database, const std::string& prefix,
                           const std::string& service, const std::string& suffix) {

    if (!conn) {
        log_err("conn invalid.");
        return -1;
    }

    if (service.empty() || database.empty() || prefix.empty() || suffix.empty()) {
        log_err("invalid param: service %s, database %s, prefix %s, suffix %s",
                service.c_str(), database.c_str(), prefix.c_str(),  suffix.c_str());
        return -1;
    }

    std::string sql = va_format(
        "CREATE TABLE IF NOT EXISTS %s.%s__%s__events_%s ( "
        "  `F_increment_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT, "
        "  `F_timestamp` bigint(20) NOT NULL COMMENT '事件上报时间， FROM_UNIXTIME可视化', "
        "  `F_entity_idx` varchar(128) NOT NULL DEFAULT '' COMMENT '多服务实例编号', "
        "  `F_metric` varchar(128) NOT NULL COMMENT '事件名称', "
        "  `F_tag` varchar(128) NOT NULL DEFAULT 'T' COMMENT '事件结果分类', "
        "  `F_step` int(11) NOT NULL DEFAULT '1' COMMENT '事件合并的时间间隔', "
        "  `F_count` int(11) NOT NULL COMMENT '当前间隔内事件总个数', "
        "  `F_value_sum` bigint(20) NOT NULL COMMENT '数值累计', "
        "  `F_value_avg` bigint(20) NOT NULL COMMENT '数值均值', "
        "  `F_value_std` bigint(20) NOT NULL COMMENT '数值方差', "
        "  `F_value_min` bigint(20) NOT NULL COMMENT 'MIN', "
        "  `F_value_max` bigint(20) NOT NULL COMMENT 'MAX', "
        "  `F_value_p50` bigint(20) NOT NULL COMMENT 'P50', "
        "  `F_value_p90` bigint(20) NOT NULL COMMENT 'P90', "
        "  `F_update_time` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP, "
        "  PRIMARY KEY (`F_increment_id`), "
        "  KEY `F_index` (`F_timestamp`, `F_metric`, `F_tag`) "
        ") ENGINE=InnoDB AUTO_INCREMENT=1 DEFAULT CHARSET=utf8; ",
        database.c_str(), prefix.c_str(), service.c_str(), suffix.c_str()
        );

    conn->sqlconn_execute_update(sql);
    return 0;
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

    std::string tag = stat.tag;
    if (tag.empty()) {
        tag = "T";
    }

    std::string table_suffix = get_table_suffix(stat.timestamp);
    std::string sql = va_format(
                   " INSERT INTO %s.%s__%s__events_%s "
                   " SET F_entity_idx = '%s', F_timestamp = %ld, "
                   " F_metric = '%s', F_tag = '%s', F_step = %d, "
                   " F_count = %d, F_value_sum = %ld, F_value_avg = %ld, F_value_std = %ld,"
                   " F_value_min = %ld, F_value_max = %ld, F_value_p50 = %ld, F_value_p90 = %ld; ",
                   database_.c_str(), table_prefix_.c_str(), stat.service.c_str(), table_suffix.c_str(),
                   stat.entity_idx.c_str(), stat.timestamp,
                   stat.metric.c_str(), tag.c_str(), stat.step,
                   stat.count, stat.value_sum, stat.value_avg, stat.value_std,
                   stat.value_min, stat.value_max, stat.value_p50, stat.value_p90);

    int nAffected = conn->sqlconn_execute_update(sql);
    if (nAffected == 1) {
        return 0;
    }

    log_notice("try create table and try again!");
    create_table(conn, database_, table_prefix_, stat.service, table_suffix.c_str());

    nAffected = conn->sqlconn_execute_update(sql);
    return nAffected == 1 ? 0 : -1;
}


std::string StoreSql::build_sql(const event_cond_t& cond, time_t linger_hint, time_t& real_start_time) {

    std::stringstream ss;
    real_start_time = ::time(NULL);
    if (cond.tm_start > 0) {
        real_start_time = std::min(::time(NULL) - linger_hint, cond.tm_start);
    } else {
        real_start_time = ::time(NULL) - linger_hint;
    }

    if (cond.groupby == GroupType::kGroupbyTimestamp) {
        ss << "SELECT IFNULL(SUM(F_count), 0), IFNULL(SUM(F_value_sum), 0), IFNULL(AVG(F_value_std), 0), "
                    " IFNULL(MIN(F_value_min), 0), IFNULL(MAX(F_value_max), 0), IFNULL(AVG(F_value_p50), 0), IFNULL(AVG(F_value_p90), 0), "
                    " IFNULL(MAX(F_step), 0), F_timestamp FROM ";
    } else if (cond.groupby == GroupType::kGroupbyTag) {
        ss << "SELECT IFNULL(SUM(F_count), 0), IFNULL(SUM(F_value_sum), 0), IFNULL(AVG(F_value_std), 0), "
                    " IFNULL(MIN(F_value_min), 0), IFNULL(MAX(F_value_max), 0), IFNULL(AVG(F_value_p50), 0), IFNULL(AVG(F_value_p90), 0), "
                    " IFNULL(MAX(F_step), 0), F_tag FROM ";
    } else {
        ss << "SELECT IFNULL(SUM(F_count), 0), IFNULL(SUM(F_value_sum), 0), IFNULL(AVG(F_value_std), 0), "
                    " IFNULL(MIN(F_value_min), 0), IFNULL(MAX(F_value_max), 0), IFNULL(AVG(F_value_p50), 0), IFNULL(AVG(F_value_p90), 0), "
                    " IFNULL(MAX(F_step), 0) FROM ";
    }

    ss << database_ << "." << table_prefix_ << "__" << cond.service << "__events_" << get_table_suffix(real_start_time) ;
    ss << " WHERE F_timestamp <= " << real_start_time <<" AND F_timestamp > " << real_start_time - cond.tm_interval;
    ss << " AND F_metric = '" << cond.metric << "'";

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
int StoreSql::select_ev_stat(const event_cond_t& cond, event_select_t& stat, time_t linger_hint) override {
    sql_conn_ptr conn;
    sql_pool_ptr_->request_scoped_conn(conn);
    if (!conn) {
        log_err("request sql conn failed!");
        return -1;
    }

    return select_ev_stat(conn, cond, stat, linger_hint);
}

int StoreSql::select_ev_stat(sql_conn_ptr& conn, const event_cond_t& cond, event_select_t& stat, time_t linger_hint) {

    if (!conn) {
        log_err("request sql conn failed!");
        return -1;
    }

    time_t real_start_time = 0;
    std::string sql = build_sql(cond, linger_hint, real_start_time);
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

    stat.timestamp = real_start_time;
    stat.tm_interval = cond.tm_interval;
    stat.service = cond.service;
    stat.metric = cond.metric;
    stat.entity_idx = cond.entity_idx;
    stat.tag = cond.tag;

      // 可能会有某个时刻没有数据的情况，这留给客户端去填充
      // 服务端不进行填充，减少网络数据的传输

    stat.summary.value_min = std::numeric_limits<int64_t>::max();
    stat.summary.value_max = std::numeric_limits<int64_t>::min();

    while (result->next()) {

        event_info_t item {};

        bool success = false;
        if (cond.groupby == GroupType::kGroupbyTimestamp) {
            success = cast_raw_value(result, 1, item.count, item.value_sum, item.value_std,
                                     item.value_min, item.value_max, item.value_p50, item.value_p90,
                                     item.step, item.timestamp);
        } else if (cond.groupby == GroupType::kGroupbyTag) {
            success = cast_raw_value(result, 1, item.count, item.value_sum, item.value_std,
                                     item.value_min, item.value_max, item.value_p50, item.value_p90,
                                     item.step, item.tag);
        } else {
            success = cast_raw_value(result, 1, item.count, item.value_sum, item.value_std,
                                     item.value_min, item.value_max, item.value_p50, item.value_p90,
                                     item.step);
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
        stat.summary.value_std += item.value_std;
        stat.summary.value_p50 += item.value_p50;
        stat.summary.value_p90 += item.value_p90;

        if (item.value_min < stat.summary.value_min) {
            stat.summary.value_min = item.value_min;
        }

        if (item.value_max > stat.summary.value_max) {
            stat.summary.value_max = item.value_max;
        }

        stat.info.push_back(item);
    }

    if (stat.summary.count != 0) {
        stat.summary.value_avg = stat.summary.value_sum / stat.summary.count;
        stat.summary.value_std = stat.summary.value_std / stat.info.size();   // not very well
        stat.summary.value_p50 = stat.summary.value_p50 / stat.info.size();
        stat.summary.value_p90 = stat.summary.value_p90 / stat.info.size();
    } else {

        // avoid display confusing value.
        stat.summary.value_min = 0;
        stat.summary.value_max = 0;
    }

    return 0;
}


int StoreSql::select_metrics(const std::string& service, std::vector<std::string>& metrics) {

    if (service.empty()) {
        log_err("select_metrics, service can not be empty!");
        return -1;
    }

    sql_conn_ptr conn;
    sql_pool_ptr_->request_scoped_conn(conn);
    if (!conn) {
        log_err("request sql conn failed!");
        return -1;
    }

    std::string table_suffix = get_table_suffix(::time(NULL));
    std::string sql = va_format(
               " SELECT distinct(F_metric) FROM %s.%s__%s__events_%s; ",
               database_.c_str(), table_prefix_.c_str(), service.c_str(), table_suffix.c_str());

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

    while (result->next()) {

        std::string t_metric;
        if(!cast_raw_value(result, 1, t_metric)) {
            log_err("raw cast failed...");
            continue;
        }

        metrics.emplace_back(t_metric);
    }

    return 0;
}





int StoreSql::select_services(std::vector<std::string>& services) {

    sql_conn_ptr conn;
    sql_pool_ptr_->request_scoped_conn(conn);
    if (!conn) {
        log_err("request sql conn failed!");
        return -1;
    }

    std::string sql = va_format(
               " SELECT table_name FROM information_schema.tables WHERE "
               " table_schema='%s' AND table_type = 'base table' AND table_name LIKE '%s%%'; ",
               database_.c_str(), table_prefix_.c_str());


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


    while (result->next()) {

        std::string t_name;
        if(!cast_raw_value(result, 1, t_name)) {
            log_err("raw cast failed...");
            continue;
        }

        if (t_name.size() > table_prefix_.size() + 2 + 15) {
            t_name = t_name.substr(table_prefix_.size() + 2);
            t_name = t_name.substr(0, t_name.size() - 15 /*"__events_xxxxxx"*/);
            services.emplace_back(t_name);
        } else {
            log_err("invalid table_name: %s", t_name.c_str());
        }
    }

    return 0;
}
