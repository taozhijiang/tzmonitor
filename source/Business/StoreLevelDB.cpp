/*-
 * Copyright (c) 2018 TAO Zhijiang<taozhijiang@gmail.com>
 *
 * Licensed under the BSD-3-Clause license, see LICENSE for full information.
 *
 */

#include <cstdlib>
#include <sstream>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <dirent.h>

#include <leveldb/comparator.h>

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
    // options.block_cache = leveldb::NewLRUCache(100 * 1048576);  // 100MB cache
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

    char cstr_key[4096] {};
    char cstr_val[4096] {};

    // key: metric#timestamp#tag#entity_idx
    // val: step#count#sum#avg#std#min#max#p50#p90
    snprintf(cstr_key, sizeof(cstr_key), "%s#%lu#%s#%s",
             stat.metric.c_str(), timestamp_exchange(stat.timestamp),
             tag.c_str(), stat.entity_idx.c_str());
    snprintf(cstr_val, sizeof(cstr_key), "%d#%d#%ld#%ld#%ld#%ld#%ld#%ld#%ld",
             stat.step, stat.count, stat.value_sum, stat.value_avg, stat.value_std,
             stat.value_min, stat.value_max, stat.value_p50, stat.value_p90);

    auto handler = get_leveldb_handler(stat.service);
    if (!handler) {
        log_err("get leveldb handler for %s failed.", stat.service.c_str());
        return -1;
    }

    leveldb::WriteOptions options;
    leveldb::Status status = handler->Put(options, std::string(cstr_key), std::string(cstr_val));
    if (!status.ok()) {
        log_err("leveldb write failed: %s - %s", cstr_key, cstr_val);
        return -1;
    }

    log_debug("leveldb service %s store %s:%s success.",
              stat.service.c_str(), cstr_key, cstr_val);
    return 0;
}

int StoreLevelDB::select_ev_stat_by_timestamp(const event_cond_t& cond, event_select_t& stat, time_t linger_hint) {

    auto handler = get_leveldb_handler(cond.service);
    if (!handler) {
        log_err("get leveldb handler for %s failed.", cond.service.c_str());
        return -1;
    }

    std::string metric_upper = cond.metric + "#";
    std::string metric_lower = cond.metric + "$";

    std::string t_upper = metric_upper + timestamp_exchange_str(stat.timestamp);
    std::string t_lower = metric_lower;
    if (stat.tm_interval > 0) {
        t_lower = metric_upper + timestamp_exchange_str(stat.timestamp - cond.tm_interval);
    }

    std::vector<std::string> vec {};
    leveldb::Options options;
    std::unique_ptr<leveldb::Iterator> it(handler->NewIterator(leveldb::ReadOptions()));

    // 聚合信息
    std::map<time_t, std::vector<event_info_t>> infos_by_timestamp;
    stat.summary.value_min = std::numeric_limits<int64_t>::max();
    stat.summary.value_max = std::numeric_limits<int64_t>::min();

    for (it->Seek(t_upper); it->Valid(); it->Next()) {

        leveldb::Slice key = it->key();
        std::string str_key = key.ToString();

        if ( options.comparator->Compare(key, t_lower) > 0) {
            log_debug("break for: %s", str_key.c_str());
            break;
        }

        // metric#timestamp#tag#entity_idx
        boost::split(vec, str_key, boost::is_any_of("#"));
        if (vec.size() != 4 ||
            vec[0].empty() || vec[1].empty() || vec[2].empty() ) {
            log_err("problem item for service %s: %s", cond.service.c_str(), str_key.c_str());
            continue;
        }

        SAFE_ASSERT(cond.metric == vec[0]);

        if (!cond.tag.empty() && vec[2] != cond.tag)
            continue;

        if (!cond.entity_idx.empty() && vec[3] != cond.entity_idx)
            continue;

        // step#count#sum#avg#std#min#max#p50#p90
        std::string str_val = it->value().ToString();
        event_info_t item {};
        if (::sscanf(str_val.c_str(), "%d#%d#%ld#%ld#%ld#%ld#%ld#%ld#%ld",
                     &item.step, &item.count, &item.value_sum, &item.value_avg, &item.value_std,
                     &item.value_min, &item.value_max, &item.value_p50, &item.value_p90) != 9) {
            log_err("scan err for: %s", str_val.c_str());
            continue;
        }

        item.timestamp = timestamp_exchange(::atoll(vec[0].c_str()));

        auto iter = infos_by_timestamp.find(item.timestamp);
        if (iter == infos_by_timestamp.end()) {
            infos_by_timestamp[item.timestamp] = std::vector<event_info_t>();
            iter = infos_by_timestamp.find(item.timestamp);
        }

        SAFE_ASSERT(iter != infos_by_timestamp.end());
        iter->second.push_back(item);

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

    }  // end for

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

    for (auto iter = infos_by_timestamp.begin(); iter != infos_by_timestamp.end(); ++iter) {

        event_info_t collect {};
        collect.timestamp = iter->first;

        collect.value_min = std::numeric_limits<int64_t>::max();
        collect.value_max = std::numeric_limits<int64_t>::min();

        for (size_t i=0; i<iter->second.size(); ++i) {
            collect.count ++;
            collect.step += iter->second[i].step;
            collect.value_sum += iter->second[i].value_sum;
            collect.value_std += iter->second[i].value_std;
            collect.value_p50 += iter->second[i].value_p50;
            collect.value_p90 += iter->second[i].value_p90;

            if (iter->second[i].value_min < collect.value_min) {
                collect.value_min = iter->second[i].value_min;
            }

            if (iter->second[i].value_max > collect.value_max) {
                collect.value_max = iter->second[i].value_max;
            }
        }

        if (collect.count > 0) {
            collect.step = collect.value_avg / collect.count;
            collect.value_avg = collect.value_sum / collect.count;
            collect.value_std = collect.value_std / collect.count;
            collect.value_p50 = collect.value_p50 / collect.count;
            collect.value_p90 = collect.value_p90 / collect.count;
        } else {
            // avoid display confusing value.
            collect.value_min = 0;
            collect.value_max = 0;
        }

        stat.info.emplace_back(collect);
    }

    return 0;
}

int StoreLevelDB::select_ev_stat_by_tag(const event_cond_t& cond, event_select_t& stat, time_t linger_hint) {

    auto handler = get_leveldb_handler(cond.service);
    if (!handler) {
        log_err("get leveldb handler for %s failed.", cond.service.c_str());
        return -1;
    }

    std::string metric_upper = cond.metric + "#";
    std::string metric_lower = cond.metric + "$";

    std::string t_upper = metric_upper + timestamp_exchange_str(stat.timestamp);
    std::string t_lower = metric_lower;
    if (stat.tm_interval > 0) {
        t_lower = metric_upper + timestamp_exchange_str(stat.timestamp - cond.tm_interval);
    }

    std::vector<std::string> vec {};
    leveldb::Options options;
    std::unique_ptr<leveldb::Iterator> it(handler->NewIterator(leveldb::ReadOptions()));

    // 聚合信息
    std::map<std::string, std::vector<event_info_t>> infos_by_tag;
    stat.summary.value_min = std::numeric_limits<int64_t>::max();
    stat.summary.value_max = std::numeric_limits<int64_t>::min();


    for (it->Seek(t_upper); it->Valid(); it->Next()) {

        leveldb::Slice key = it->key();
        std::string str_key = key.ToString();

        if ( options.comparator->Compare(key, t_lower) > 0) {
            log_debug("break for: %s", str_key.c_str());
            break;
        }

        // metric#timestamp#tag#entity_idx
        boost::split(vec, str_key, boost::is_any_of("#"));
        if (vec.size() != 4 ||
            vec[0].empty() || vec[1].empty() || vec[2].empty() ) {
            log_err("problem item for service %s: %s", cond.service.c_str(), str_key.c_str());
            continue;
        }

        SAFE_ASSERT(cond.metric == vec[0]);

        if (!cond.tag.empty() && vec[2] != cond.tag)
            continue;

        if (!cond.entity_idx.empty() && vec[3] != cond.entity_idx)
            continue;

        // step#count#sum#avg#std
        std::string str_val = it->value().ToString();
        event_info_t item {};
        if (::sscanf(str_val.c_str(), "%d#%d#%ld#%ld#%f",
                     &item.step, &item.count, &item.value_sum, &item.value_avg, &item.value_std) != 5) {
            log_err("scan err for: %s", str_val.c_str());
            continue;
        }

        item.tag = vec[2];

        auto iter = infos_by_tag.find(item.tag);
        if (iter == infos_by_tag.end()) {
            infos_by_tag[item.tag] = std::vector<event_info_t>();
            iter = infos_by_tag.find(item.tag);
        }

        SAFE_ASSERT(iter != infos_by_tag.end());
        iter->second.push_back(item);

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
    } // end for


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


    for (auto iter = infos_by_tag.begin(); iter != infos_by_tag.end(); ++iter) {

        event_info_t collect {};
        collect.tag = iter->first;

        collect.value_min = std::numeric_limits<int64_t>::max();
        collect.value_max = std::numeric_limits<int64_t>::min();

        for (size_t i=0; i<iter->second.size(); ++i) {
            collect.count ++;
            collect.step += iter->second[i].step;
            collect.value_sum += iter->second[i].value_sum;
            collect.value_std += iter->second[i].value_std;
            collect.value_p50 += iter->second[i].value_p50;
            collect.value_p90 += iter->second[i].value_p90;

            if (iter->second[i].value_min < collect.value_min) {
                collect.value_min = iter->second[i].value_min;
            }

            if (iter->second[i].value_max > collect.value_max) {
                collect.value_max = iter->second[i].value_max;
            }
        }

        if (collect.count > 0) {
            collect.step = collect.value_avg / collect.count;
            collect.value_avg = collect.value_sum / collect.count;
            collect.value_std = collect.value_std / collect.count;
            collect.value_p50 = collect.value_p50 / collect.count;
            collect.value_p90 = collect.value_p90 / collect.count;
        } else {
            // avoid display confusing value.
            collect.value_min = 0;
            collect.value_max = 0;
        }

        stat.info.emplace_back(collect);
    }

    return 0;

}

int StoreLevelDB::select_ev_stat_by_none(const event_cond_t& cond, event_select_t& stat, time_t linger_hint) {

    auto handler = get_leveldb_handler(cond.service);
    if (!handler) {
        log_err("get leveldb handler for %s failed.", cond.service.c_str());
        return -1;
    }

    std::string metric_upper = cond.metric + "#";
    std::string metric_lower = cond.metric + "$";

    std::string t_upper = metric_upper + timestamp_exchange_str(stat.timestamp);
    std::string t_lower = metric_lower;
    if (stat.tm_interval > 0) {
        t_lower = metric_upper + timestamp_exchange_str(stat.timestamp - cond.tm_interval);
    }

    std::vector<std::string> vec {};
    leveldb::Options options;
    std::unique_ptr<leveldb::Iterator> it(handler->NewIterator(leveldb::ReadOptions()));

    stat.summary.value_min = std::numeric_limits<int64_t>::max();
    stat.summary.value_max = std::numeric_limits<int64_t>::min();

    for (it->Seek(t_upper); it->Valid(); it->Next()) {

        leveldb::Slice key = it->key();
        std::string str_key = key.ToString();

        if ( options.comparator->Compare(key, t_lower) > 0) {
            log_debug("break for: %s", str_key.c_str());
            break;
        }

        // metric#timestamp#tag#entity_idx
        boost::split(vec, str_key, boost::is_any_of("#"));
        if (vec.size() != 4 ||
            vec[0].empty() || vec[1].empty() || vec[2].empty() ) {
            log_err("problem item for service %s: %s", cond.service.c_str(), str_key.c_str());
            continue;
        }

        SAFE_ASSERT(cond.metric == vec[0]);

        if (!cond.tag.empty() && vec[2] != cond.tag)
            continue;

        if (!cond.entity_idx.empty() && vec[3] != cond.entity_idx)
            continue;

        // step#count#sum#avg#std
        std::string str_val = it->value().ToString();
        event_info_t item {};
        if (::sscanf(str_val.c_str(), "%d#%d#%ld#%ld#%ld#%ld#%ld#%ld#%ld",
                     &item.step, &item.count, &item.value_sum, &item.value_avg, &item.value_std,
                     &item.value_min, &item.value_max, &item.value_p50, &item.value_p90) != 9) {
            log_err("scan err for: %s", str_val.c_str());
            continue;
        }

        stat.summary.count += item.count;
        stat.summary.value_sum += item.value_sum;
        stat.summary.value_std += item.value_std;
        stat.summary.value_p50 += item.value_p50;
        stat.summary.value_p90 += item.value_p90;

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


// group summary
int StoreLevelDB::select_ev_stat(const event_cond_t& cond, event_select_t& stat, time_t linger_hint) override {

    if (cond.service.empty()) {
        log_err("error check error!");
        return -1;
    }

    stat.timestamp = ::time(NULL);
    if (cond.tm_start > 0) {
        stat.timestamp = std::min(::time(NULL) - linger_hint, cond.tm_start);
    } else {
        stat.timestamp = ::time(NULL) - linger_hint;
    }

    stat.service = cond.service;
    stat.tm_interval = cond.tm_interval;
    stat.service = cond.service;
    stat.metric = cond.metric;
    stat.entity_idx = cond.entity_idx;
    stat.tag = cond.tag;

    if (cond.groupby == GroupType::kGroupbyTimestamp) {
        return select_ev_stat_by_timestamp(cond, stat, linger_hint);
    }
    else if (cond.groupby == GroupType::kGroupbyTag) {
        return select_ev_stat_by_tag(cond, stat, linger_hint);
    }
    else {
        return select_ev_stat_by_none(cond, stat, linger_hint);
    }

    return -1;
}


int StoreLevelDB::select_metrics(const std::string& service, std::vector<std::string>& metrics) {

    if (service.empty()) {
        log_err("select_metrics, service can not be empty!");
        return -1;
    }

    auto handler = get_leveldb_handler(service);
    if (!handler) {
        log_err("get leveldb handler for %s failed.", service.c_str());
        return -1;
    }

    std::unique_ptr<leveldb::Iterator> it(handler->NewIterator(leveldb::ReadOptions()));

    std::set<std::string> unique_metrics;
    std::vector<std::string> vec{};
    std::string cached_metric;
    for (it->SeekToFirst(); it->Valid(); it->Next()) {

        std::string str_key = it->key().ToString();

        // metric#timestamp#tag#entity_idx
        boost::split(vec, str_key, boost::is_any_of("#"));
        if (vec.size() != 4 ||
            vec[0].empty() || vec[1].empty() || vec[2].empty() ) {
            log_err("problem item for service %s: %s", service.c_str(), str_key.c_str());
            continue;
        }

        if (cached_metric != vec[0]) {
            cached_metric = vec[0];
            unique_metrics.insert(vec[0]);
        }
    }


    metrics.clear();
    metrics.assign(unique_metrics.cbegin(),  unique_metrics.cend());

    return 0;
}


int StoreLevelDB::select_services(std::vector<std::string>& services) {


    // 遍历目录，获取所有服务

    DIR *d = NULL;
    struct dirent* d_item = NULL;
    struct stat sb;

    if (!(d = opendir(filepath_.c_str()))) {
        log_err("opendir for %s failed.",  filepath_.c_str());
        return -1;
    }


    std::vector<std::string> leveldb_files {};

    while ( (d_item = readdir(d)) != NULL ) {

        // 跳过隐藏目录
        if (::strncmp(d_item->d_name, ".", 1) == 0) {
            continue;
        }

        // 取出所有目录
        if (stat(d_item->d_name, &sb) == 0 && S_ISDIR(sb.st_mode)) {
            // 满足前缀
            if (::strncmp(d_item->d_name, table_prefix_.c_str(), table_prefix_.size()) == 0) {
                leveldb_files.push_back(d_item->d_name);
            }
        }
    }

    services.clear();

    for (size_t i=0; i<leveldb_files.size(); ++i) {

        std::string t_name = leveldb_files[i];

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
