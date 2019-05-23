/*-
 * Copyright (c) 2019 TAO Zhijiang<taozhijiang@gmail.com>
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

#include <string/StrUtil.h>

// leveldb 存储表设计思路
// heracles/heracles__service__events_201902
//           key: timestamp#metric#tag#entity_idx
//           val: step#count#sum#avg#min#max#p10#p50#p90


// packed存储，不补齐
// 为了保证迁移后数据的一致性，所有数据底层存储采用网络字节序

struct leveldb_internal_layout_t {

    uint8_t d;    // 'D'
    uint8_t step;
    int32_t count;
    int64_t sum;
    int32_t avg;
    int32_t min;
    int32_t max;
    int32_t p10;
    int32_t p50;
    int32_t p90;

    std::string dump() const {
        char msg[128]{};
        snprintf(msg, sizeof(msg), "leveldb: %c,step:%d,count:%d,sum:%ld,avg:%d.min:%d,max:%d,p10:%d,p50%d,p90%d",
                 d, step, count, sum, avg, min, max, p10, p50, p90);
        return msg;
    }

    void from_net_endian() {
        count = be32toh(count);
        sum = be64toh(sum);
        avg = be32toh(avg);
        min = be32toh(min);
        max = be32toh(max);
        p10 = be32toh(p10);
        p50 = be32toh(p50);
        p90 = be32toh(p90);
    }

    void to_net_endian() {
        count = htobe32(count);
        sum = htobe64(sum);
        avg = htobe32(avg);
        min = htobe32(min);
        max = htobe32(max);
        p10 = htobe32(p10);
        p50 = htobe32(p50);
        p90 = htobe32(p90);
    }

} __attribute__((packed));


class StoreLevelDB : public StoreIf {

public:
    StoreLevelDB() :
        lock_(),
        levelDBs_(),
        filepath_(),
        table_prefix_() {
    }

public:

    bool init(const libconfig::Config& conf)override;

    int insert_ev_stat(const event_insert_t& stat)override;
    int select_ev_stat(const event_cond_t& cond, event_select_t& stat, time_t linger_hint)override;

    int select_metrics(const std::string& service, std::vector<std::string>& metrics)override;
    int select_services(std::vector<std::string>& services)override;

private:

    int select_ev_stat_by_timestamp(const event_cond_t& cond, event_select_t& stat, time_t linger_hint);
    int select_ev_stat_by_tag(const event_cond_t& cond, event_select_t& stat, time_t linger_hint);
    int select_ev_stat_by_none(const event_cond_t& cond, event_select_t& stat, time_t linger_hint);

    time_t timestamp_exchange(time_t t) {
        return 9999999999 - t;
    }

    std::string timestamp_exchange_str(time_t t) {
        time_t n = 9999999999 - t;
        return roo::StrUtil::to_string(n);
    }

    std::string get_table_suffix(time_t time_sec) {

        struct tm now_time;
        localtime_r(&time_sec, &now_time);

        char buff[20] = { 0, };
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
