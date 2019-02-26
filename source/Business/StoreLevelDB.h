/*-
 * Copyright (c) 2018 TAO Zhijiang<taozhijiang@gmail.com>
 *
 * Licensed under the BSD-3-Clause license, see LICENSE for full information.
 *
 */


#ifndef __BUSINESS_STORE_LEVELDB_H__
#define __BUSINESS_STORE_LEVELDB_H__

#include <Business/StoreIf.h>

#include <mutex>
#include <memory>
#include <leveldb/db.h>

// leveldb 存储表设计思路
// tzmonitor/tzmonitor__service__events_201902
//           key: timestamp#metric#tag#entity_idx
//           val: step#count#sum#avg#std



class StoreLevelDB: public StoreIf {

public:
    StoreLevelDB():
        lock_(),
        levelDBs_(),
        filepath_(),
        table_prefix_() {
    }

public:

    bool init(const libconfig::Config& conf);

    int insert_ev_stat(const event_insert_t& stat);
    int select_ev_stat(const event_cond_t& cond, event_select_t& stat, time_t linger_hint);

    int select_metrics(const std::string& service, std::vector<std::string>& metrics);
    int select_services(std::vector<std::string>& services);

private:

    std::string get_table_suffix(time_t time_sec) {

        struct tm now_time;
        localtime_r(&time_sec, &now_time);

        char buff[20] = {0, };
        sprintf(buff, "%04d%02d", now_time.tm_year + 1900, now_time.tm_mon + 1);

        return buff;
    }

    std::shared_ptr<leveldb::DB> get_leveldb_handler(const std::string& service);


    std::mutex lock_;

    // leveldb::DB 指针可以被多线程使用，内部保证了线程安全
    // key: service
    struct leveldb_handler_t {
        std::string current_suffix_;
        std::shared_ptr<leveldb::DB> handler_;
    };

    typedef std::map<std::string, std::shared_ptr<leveldb_handler_t>> leveldb_handlers_t;
    std::shared_ptr<leveldb_handlers_t> levelDBs_;

    std::string filepath_;
    std::string table_prefix_;
};

#endif // __BUSINESS_STORE_LEVELDB_H__
