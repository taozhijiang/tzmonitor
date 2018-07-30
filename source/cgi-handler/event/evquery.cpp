#include <string>
#include <memory>
#include <boost/algorithm/string.hpp>
#include <json/json.h>

#include <connect/SqlConn.h>
#include <core/EventTypes.h>
#include <tzhttpd/HttpProto.h>
#include <tzhttpd/CgiHelper.h>
#include <tzhttpd/Log.h>

#include <tzhttpd/StrUtil.h>
using tzhttpd::StrUtil;

#include "ErrorDef.h"

// TODO: read from config file
time_t event_linger_        = 5;
std::string database_       = "bankpay";
std::string table_prefix_   = "t_tzmonitor_";

std::string mysql_hostname_ = "127.0.0.1";
int mysql_port_ = 3306;
std::string mysql_username_ = "root";
std::string mysql_passwd_   = "1234";
std::string mysql_database_ = "bankpay";
//

// SQL pool
std::shared_ptr<ConnPool<SqlConn, SqlConnPoolHelper>> sql_pool_ptr_;


static std::string build_sql(const event_cond_t& cond, time_t& start_time);
static int query_ev_stat(const event_cond_t& cond, event_query_t& stat);
int ev_query_handler(const std::string& request, std::string& response, std::string& add_header);

static std::string get_table_suffix(time_t time_sec) {
    struct tm now_time;
    localtime_r(&time_sec, &now_time);

    char buff[20] = {0, };
    sprintf(buff, "%04d%02d", now_time.tm_year + 1900, now_time.tm_mon + 1);

    return buff;
}

#ifdef __cplusplus
extern "C" {
#endif


int module_init() {

    SqlConnPoolHelper helper(mysql_hostname_, mysql_port_,
                             mysql_username_, mysql_passwd_, mysql_database_);
    sql_pool_ptr_.reset(new ConnPool<SqlConn, SqlConnPoolHelper>("MySQLPool", 60, helper, 60 /*60s*/));
    if (!sql_pool_ptr_ || !sql_pool_ptr_->init()) {
        tzhttpd::tzhttpd_log_err("Init SqlConnPool failed!");
        return -1;
    }

    return 0;
}

int module_exit() {
    return 0;
}


int cgi_get_handler(const msg_t* param, msg_t* rsp, msg_t* rsp_header) {
    std::string request = std::string(param->data, param->len);
    std::string response;
    std::string add_header;

    int ret_code = ev_query_handler(request, response, add_header);

    if(!response.empty()) {
        fill_msg(rsp, response.c_str(), response.size());
    }

    if (!add_header.empty()) {
        fill_msg(rsp_header, add_header.c_str(), add_header.size());
    }

    return ret_code;
}


#ifdef __cplusplus
}
#endif



int ev_query_handler(const std::string& request, std::string& response, std::string& add_header) {

    do {

        event_cond_t cond {};
        tzhttpd::tzhttpd_log_debug("recv request: %s", request.c_str());

        {
            Json::Value root;
            Json::Reader reader;
            if (!reader.parse(request, root) || root.isNull()) {
                tzhttpd::tzhttpd_log_err("parse error: %s", request.c_str());
                break;
            }

            // required
            if (!root["version"].isString() || !root["name"].isString() ||
                !root["interval_sec"].isString() ) {
                tzhttpd::tzhttpd_log_err("required param is missing.");
                break;
            }

            std::map<std::string, std::string> map_param;
            Json::Value::Members mem = root.getMemberNames();
            for (Json::Value::Members::const_iterator iter = mem.begin(); iter != mem.end(); iter++) {
                map_param[*iter] = root[*iter].asString();
            }

            cond.version = map_param["version"];
            cond.name = map_param["name"];
            cond.interval_sec = ::atoll(map_param["interval_sec"].c_str());

            if (cond.version.empty() || cond.name.empty() || cond.interval_sec <= 0) {
                tzhttpd::tzhttpd_log_err("required param missing...");
                break;
            }

            // optional

            if(map_param.find("start") != map_param.end()) {
                cond.start = ::atoll(map_param["start"].c_str());
            }

            if(map_param.find("host") != map_param.end()) {
                cond.host = map_param["host"];
            }

            if(map_param.find("serv") != map_param.end()) {
                cond.serv = map_param["serv"];
            }

            if(map_param.find("entity_idx") != map_param.end()) {
                cond.entity_idx = map_param["entity_idx"];
            }

            if(map_param.find("flag") != map_param.end()) {
                cond.flag = map_param["flag"];
            }

            if(map_param.find("groupby") != map_param.end()) {
                if (boost::iequals(map_param["groupby"], "time")) {
                    cond.groupby = GroupType::kGroupbyTime;
                } else if (boost::iequals(map_param["groupby"], "flag")) {
                    cond.groupby = GroupType::kGroupbyFlag;
                }
            }

        }


        event_query_t stat{};
        if (query_ev_stat(cond, stat) != ErrorDef::OK) {
            tzhttpd::tzhttpd_log_err("call get_event detail failed!");
            break;
        }

        {

             // build request json
            Json::Value root;
            root["version"] = "1.0.0";
            root["name"] = cond.name;
            root["time"] = StrUtil::convert_to_string(stat.time);
            root["interval_sec"] = StrUtil::convert_to_string(cond.interval_sec);

            if(!cond.host.empty()) root["host"] = cond.host;
            if(!cond.serv.empty()) root["serv"] = cond.serv;
            if(!cond.entity_idx.empty()) root["entity_idx"] = cond.entity_idx;
            if(!cond.flag.empty()) root["flag"] = cond.flag;

            Json::Value summary;
            Json::FastWriter fast_writer;

            summary["count"] = StrUtil::convert_to_string(stat.summary.count);
            summary["value_sum"] = StrUtil::convert_to_string(stat.summary.value_sum);
            summary["value_avg"] = StrUtil::convert_to_string(stat.summary.value_avg);
            summary["value_std"] = StrUtil::convert_to_string(stat.summary.value_std);
            root["summary"] = fast_writer.write(summary);

            if (cond.groupby != GroupType::kGroupNone) {
                Json::Value ordersJson;
                for (auto iter = stat.info.begin(); iter != stat.info.end(); ++iter) {
                    Json::Value orderjson{};

                    if (cond.groupby == GroupType::kGroupbyTime) {
                        orderjson["time"] = StrUtil::convert_to_string(iter->time);
                    } else if (cond.groupby == GroupType::kGroupbyFlag) {
                        orderjson["flag"] = StrUtil::convert_to_string(iter->flag);
                    }

                    orderjson["count"] = StrUtil::convert_to_string(iter->count);
                    orderjson["value_sum"] = StrUtil::convert_to_string(iter->value_sum);
                    orderjson["value_avg"] = StrUtil::convert_to_string(iter->value_avg);
                    orderjson["value_std"] = StrUtil::convert_to_string(iter->value_std);

                    ordersJson.append(orderjson);
                }

                root["info"] = fast_writer.write(ordersJson);
            }

            response = fast_writer.write(root);
            tzhttpd::tzhttpd_log_debug("response: %s", response.c_str());
        }

        add_header = {"Content-Type: text/html; charset=utf-8"};

        return ErrorDef::OK;

    } while (0);

    response = tzhttpd::http_proto::content_error;
    add_header = {"Content-Type: text/html; charset=utf-8"};

    return ErrorDef::Error;
}


static std::string build_sql(const event_cond_t& cond, time_t& start_time) {

    start_time = cond.start;
    std::stringstream ss;
    if (cond.start <= 0) {
        start_time = ::time(NULL) - 5;  // hardcode temporary
    }

    if (cond.groupby == GroupType::kGroupbyTime) {
        ss << "SELECT IFNULL(SUM(F_count), 0), IFNULL(SUM(F_value_sum), 0), IFNULL(AVG(F_value_std), 0), F_time FROM ";
    } else if (cond.groupby == GroupType::kGroupbyFlag) {
        ss << "SELECT IFNULL(SUM(F_count), 0), IFNULL(SUM(F_value_sum), 0), IFNULL(AVG(F_value_std), 0), F_flag FROM ";
    } else {
        ss << "SELECT IFNULL(SUM(F_count), 0), IFNULL(SUM(F_value_sum), 0), IFNULL(AVG(F_value_std), 0) FROM ";
    }

    ss << database_ << "." << table_prefix_ << "event_stat_" << get_table_suffix(start_time) ;
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

    if (cond.groupby == GroupType::kGroupbyTime) {
        ss << " GROUP BY F_time ORDER BY F_time DESC; ";
    } else if (cond.groupby == GroupType::kGroupbyFlag) {
        ss << " GROUP BY F_flag; ";
    }

    std::string sql = ss.str();
    log_info("query str: %s", sql.c_str());

    return sql;
}

static int query_ev_stat(const event_cond_t& cond, event_query_t& stat) {
    sql_conn_ptr conn;
    sql_pool_ptr_->request_scoped_conn(conn);
    if (!conn) {
        printf("request sql conn failed!");
        return ErrorDef::DatabasePoolErr;
    }

    time_t real_start_time = 0;
    std::string sql = build_sql(cond, real_start_time);
    stat.time = real_start_time;

    shared_result_ptr result;
    result.reset(conn->sqlconn_execute_query(sql));
    if (!result) {
        printf("Failed to query info: %s", sql.c_str());
        return ErrorDef::DatabaseExecErr;
    }

    if (result->rowsCount() == 0) {
        log_info("Empty record found!");
        return ErrorDef::OK;
    }

      // 可能会有某个时刻没有数据的情况，这留给客户端去填充
      // 服务端不进行填充，减少网络数据的传输
    while (result->next()) {

        event_info_t item {};

        bool success = false;
        if (cond.groupby == GroupType::kGroupbyTime) {
            success = cast_raw_value(result, 1, item.count, item.value_sum, item.value_std, item.time);
        } else if (cond.groupby == GroupType::kGroupbyFlag) {
            success = cast_raw_value(result, 1, item.count, item.value_sum, item.value_std, item.flag);
        } else {
            success = cast_raw_value(result, 1, item.count, item.value_sum, item.value_std);
        }

        if (!success) {
            printf("Failed to cast info ..." );
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

    return ErrorDef::OK;
}