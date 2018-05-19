
#include "General.h"
#include "EventSql.h"

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

    return ErrorDef::NotImplmented;
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
