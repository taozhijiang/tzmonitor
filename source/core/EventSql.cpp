
#include "General.h"
#include "EventSql.h"
#include "EventRepos.h"

#include "Helper.h"

#include <utils/Log.h>

namespace EventSql {

std::string database;
std::string table_prefix;


static std::string get_table_suffix(time_t time_sec) {
    struct tm now_time;
    localtime_r(&time_sec, &now_time);

    char buff[20] = {0, };
    sprintf(buff, "%04d%02d", now_time.tm_year + 1900, now_time.tm_mon + 1);

    return buff;
}

int insert_ev_stat(const ev_stat_t& stat) {

    sql_conn_ptr conn;
    helper::request_scoped_sql_conn(conn);
    if (!conn){
        log_err("request sql conn failed!");
        return ErrorDef::DatabasePoolErr;
    }

    return insert_ev_stat(conn, stat);
}


int insert_ev_stat(sql_conn_ptr& conn, const ev_stat_t& stat) {

    if (!conn){
        log_err("request sql conn failed!");
        return ErrorDef::DatabasePoolErr;
    }

    if (stat.host.empty() || stat.serv.empty() || stat.name.empty()) {
        log_err("error check error!");
        return ErrorDef::ParamErr;
    }

    // set default value
    std::string entity_idx = "1";
    std::string flag = "T";
    if (!stat.entity_idx.empty()) {
        entity_idx = stat.entity_idx;
    }

    if (!stat.flag.empty()) {
        flag = stat.flag;
    }

    std::string sql = va_format(
                   " INSERT INTO %s.%sevent_stat_%s "
                   " SET F_host = '%s', F_serv = '%s', F_entity_idx = '%s', F_time = %ld, "
                   " F_name = '%s', F_flag = '%s', "
                   " F_count = %d, F_value_sum = %ld, F_value_avg = %ld, F_value_std = %f; ",
                   database.c_str(), table_prefix.c_str(), get_table_suffix(stat.time).c_str(),
                   stat.host.c_str(), stat.serv.c_str(), entity_idx.c_str(), stat.time,
                   stat.name.c_str(), flag.c_str(),
                   stat.count, stat.value_sum, stat.value_avg, stat.value_std);

    int nAffected = conn->sqlconn_execute_update(sql);
    return nAffected == 1 ? 0 : ErrorDef::DatabaseExecErr;
}


// group summary
int query_ev_stat(const ev_cond_t& cond, ev_stat_t& stat) {
    sql_conn_ptr conn;
    helper::request_scoped_sql_conn(conn);
    if (!conn){
        log_err("request sql conn failed!");
        return ErrorDef::DatabasePoolErr;
    }

    return query_ev_stat(conn, cond, stat);
}

int query_ev_stat(sql_conn_ptr& conn, const ev_cond_t& cond, ev_stat_t& stat) {

    if (!conn){
        log_err("request sql conn failed!");
        return ErrorDef::DatabasePoolErr;
    }

    time_t start_time = cond.start;
    std::stringstream ss;
    if (cond.start <= 0) {
        start_time = ::time(NULL) - EventRepos::instance().get_event_linger();
    }

    ss << "SELECT IFNULL(SUM(F_count), 0), IFNULL(SUM(F_value_sum), 0), IFNULL(AVG(F_value_std), 0) FROM " ;
    ss << database << "." << table_prefix << "event_stat_" << get_table_suffix(start_time) ;
    ss << " WHERE F_time <= " << start_time <<" AND F_time > " << start_time - cond.interval_sec;
    ss << " AND F_name = '" << cond.name << "'";

    if (!cond.host.empty()) {
        ss << " AND F_host = '" << cond.host << "'";
    }

    if (!cond.serv.empty()) {
        ss << " AND F_serv = '" << cond.serv << "'";
    }

    if (!cond.entity_idx.empty()) {
        ss << " AND F_entity_idx = '" << cond.entity_idx << "'";
    }

    if (!cond.flag.empty()) {
        ss << " AND F_flag = '" << cond.flag << "'";
    }

    std::string sql = ss.str();
    log_debug("query str: %s", sql.c_str());

    shared_result_ptr result;
    result.reset(conn->sqlconn_execute_query(sql));
    if (!result || !result->next()) {
        log_err("Failed to query info: %s", sql.c_str());
        return ErrorDef::DatabaseExecErr;
    }

    if (!cast_raw_value(result, 1, stat.count, stat.value_sum, stat.value_std)){
        log_err("Failed to cast info ..." );
        return ErrorDef::DatabaseResultErr;
    }

    stat.time = start_time;  // 真正开始取数的事件点
    if (stat.count != 0) {
        stat.value_avg = stat.value_sum / stat.count;
    } else {
        stat.value_avg = 0;
    }
    return ErrorDef::OK;
}

// detailed
// may never be used in practice
int query_ev_stat(const ev_cond_t& cond, std::vector<ev_stat_t>& stats) {
    sql_conn_ptr conn;
    helper::request_scoped_sql_conn(conn);
    if (!conn){
        log_err("request sql conn failed!");
        return ErrorDef::DatabasePoolErr;
    }

    return query_ev_stat(conn, cond, stats);
}

int query_ev_stat(sql_conn_ptr& conn, const ev_cond_t& cond, std::vector<ev_stat_t>& stats) {

    return ErrorDef::NotImplmented;
}


} // end namespace
