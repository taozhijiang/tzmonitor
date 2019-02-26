/*-
 * Copyright (c) 2018 TAO Zhijiang<taozhijiang@gmail.com>
 *
 * Licensed under the BSD-3-Clause license, see LICENSE for full information.
 *
 */

#include <cstdlib>
#include <sstream>

#include <Utils/Log.h>
#include <Utils/StrUtil.h>

#include <Business/StoreLevelDB.h>

using namespace tzrpc;


static std::shared_ptr<leveldb::DB> NULLPTR_HANDLER;

bool StoreLevelDB::init(const libconfig::Config& conf) override {

    if (!conf.lookupValue("rpc_business.leveldb.filepath", filepath_) ||
        !conf.lookupValue("rpc_business.leveldb.table_prefix", table_prefix_) ||
        filepath_.empty() || table_prefix_.empty() )
    {
        log_err("Error, get level configure value error");
        return false;
    }

        // check dest exist?
    if (::access(filepath_.c_str(), W_OK) != 0) {
        log_err("access filepath_ %s failed.", filepath_.c_str());
        return false;
    }

    levelDBs_.reset(new leveldb_handlers_t());
    if (!levelDBs_) {
        log_err("create empty levelDBs_ failed.");
        return false;
    }

    NULLPTR_HANDLER.reset();

    return true;
}


std::shared_ptr<leveldb::DB> StoreLevelDB::get_leveldb_handler(const std::string& service) {


    if (service.empty()) {
        log_err("service can not be empty!");
        return NULLPTR_HANDLER;
    }

    std::shared_ptr<leveldb_handlers_t> handlers;
    {
        std::lock_guard<std::mutex> lock(lock_);
        handlers = levelDBs_;
    }

    time_t now = ::time(NULL);
    std::string now_suffix = get_table_suffix(now);

    auto handler = handlers->find(service);
    if (handler != handlers->end()) {
        if (now_suffix == handler->second->current_suffix_ &&
            handler->second->handler_) {
            return handler->second->handler_;
        } else {
            log_err("suffix and handler check failed, hold %s, now %s",
                    handler->second->current_suffix_.c_str(), now_suffix.c_str());
        }
    } else {
        log_err("service %s not created.", service.c_str());
    }

do_create:

    std::lock_guard<std::mutex> lock(lock_);

    // check again
    handler = levelDBs_->find(service);
    if (handler != levelDBs_->end()) {
        if (now_suffix == handler->second->current_suffix_ &&
            handler->second->handler_) {
            log_notice("good, hit for service %s with suffix %s",
                       service.c_str(), handler->second->current_suffix_.c_str());
            return handler->second->handler_;
        }
    }

    // do create

    char fullpath[PATH_MAX] {};
    snprintf(fullpath, PATH_MAX, "%s/%s__%s__events_%s",
             filepath_.c_str(), table_prefix_.c_str(), service.c_str(), now_suffix.c_str());

    leveldb::Options options;
    options.create_if_missing = true;
    leveldb::DB* db;
    leveldb::Status status = leveldb::DB::Open(options, fullpath, &db);

    if (!status.ok()) {
        log_err("Open levelDB %s failed.", fullpath);
        return NULLPTR_HANDLER;
    }

    std::shared_ptr<leveldb_handler_t> new_handler = std::make_shared<leveldb_handler_t>();
    if (!new_handler) {
        log_err("create new handler for %s failed.", fullpath);
        return NULLPTR_HANDLER;
    }

    new_handler->current_suffix_ = now_suffix;
    new_handler->handler_.reset(db);

    // do add
    (*levelDBs_)[service] = new_handler;

    log_notice("success add for service %s with suffix %s, fullpath: %s",
               service.c_str(), now_suffix.c_str(), fullpath);
    return new_handler->handler_;
}


int StoreLevelDB::insert_ev_stat(const event_insert_t& stat) override {

    if (stat.service.empty() || stat.metric.empty() || stat.timestamp == 0) {
        log_err("error check error!");
        return -1;
    }

    std::string tag = stat.tag;
    if (tag.empty()) {
        tag = "T";
    }

    char str_key[4096] {};
    char str_val[4096] {};

    // key: timestamp#metric#tag#entity_idx
    // val: step#count#sum#avg#std
    snprintf(str_key, sizeof(str_key), "%lu#%s#%s#%s",
             stat.timestamp, stat.metric.c_str(), tag.c_str(), stat.entity_idx.c_str());
    snprintf(str_val, sizeof(str_key), "%d#%d#%ld#%ld#%f",
             stat.step, stat.count, stat.value_sum, stat.value_avg, stat.value_std);

    auto handler = get_leveldb_handler(stat.service);
    if (!handler) {
        log_err("get leveldb handler for %s failed.", stat.service.c_str());
        return -1;
    }

    leveldb::WriteOptions options;
    leveldb::Status status = handler->Put(options, std::string(str_key), std::string(str_val));
    if (!status.ok()) {
        log_err("leveldb write failed: %s - %s", str_key, str_val);
        return -1;
    }

    return 0;
}


#if 0

std::string StoreSql::build_sql(const event_cond_t& cond, time_t linger_hint, time_t& real_start_time) {

    std::stringstream ss;
    real_start_time = ::time(NULL);
    if (cond.tm_start > 0) {
        real_start_time = std::min(::time(NULL) - linger_hint, cond.tm_start);
    } else {
        real_start_time = ::time(NULL) - linger_hint;
    }

    if (cond.groupby == GroupType::kGroupbyTimestamp) {
        ss << "SELECT IFNULL(SUM(F_count), 0), IFNULL(SUM(F_value_sum), 0), IFNULL(AVG(F_value_std), 0), F_timestamp FROM ";
    } else if (cond.groupby == GroupType::kGroupbyTag) {
        ss << "SELECT IFNULL(SUM(F_count), 0), IFNULL(SUM(F_value_sum), 0), IFNULL(AVG(F_value_std), 0), F_tag FROM ";
    } else {
        ss << "SELECT IFNULL(SUM(F_count), 0), IFNULL(SUM(F_value_sum), 0), IFNULL(AVG(F_value_std), 0) FROM ";
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

#endif

// group summary
int StoreLevelDB::select_ev_stat(const event_cond_t& cond, event_select_t& stat, time_t linger_hint) override {

#if 0

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
#endif

    return 0;
}


int StoreLevelDB::select_metrics(const std::string& service, std::vector<std::string>& metrics) {

    #if 0

    sql_conn_ptr conn;
    sql_pool_ptr_->request_scoped_conn(conn);
    if (!conn) {
        log_err("request sql conn failed!");
        return -1;
    }

    if (service.empty()) {
        log_err("select_metrics, service can not be empty!");
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

    #endif

    return 0;
}


int StoreLevelDB::select_services(std::vector<std::string>& services) {


    #if 0


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
    #endif

    return 0;
}
