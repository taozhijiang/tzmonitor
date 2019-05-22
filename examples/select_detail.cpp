/*-
 * Copyright (c) 2019 TAO Zhijiang<taozhijiang@gmail.com>
 *
 * Licensed under the BSD-3-Clause license, see LICENSE for full information.
 *
 */

#include <unistd.h>
#include <string>
#include <iostream>
#include <syslog.h>


#include <ctime>
#include <cstring>
time_t to_unixstamp(std::string timeStamp) {  
    struct tm tm {};  
    ::memset(&tm, 0, sizeof(tm));  
      
    sscanf(timeStamp.c_str(), "%d-%d-%d %d:%d:%d",   
           &tm.tm_year, &tm.tm_mon, &tm.tm_mday,  
           &tm.tm_hour, &tm.tm_min, &tm.tm_sec);  
  
    tm.tm_year -= 1900;  
    tm.tm_mon--;  
  
    return ::mktime(&tm);  
}

#include <Client/include/HeraclesClient.h>

std::shared_ptr<heracles_client::HeraclesClient> selecter_;

static int select_by_tag(const std::string& service, const std::string& metric,
                         int64_t tm, int64_t tm_start) {
    
    event_cond_t cond {};
    cond.tm_start = tm_start;
    cond.tm_interval = tm;
    cond.service = service;
    cond.metric  = metric;
    cond.groupby = GroupType::kGroupbyTag;
    
    event_select_t select {};

    int code = selecter_->select_stat(cond, select);
    if( code != 0) {
        std::cout << "select fail for " << service << ", " << metric << std::endl;
        return -1;
    }
    
    std::cout << "select_stat_by_tag info:" << std::endl;
    
    std::cout << "select condition return:" << std::endl;
    std::cout << "    timestamp:" << select.timestamp << ", ";
    std::cout << "tm_interval:" << select.tm_interval << ", ";
    std::cout << "service:" << select.service << ", ";
    std::cout << "metric:" << select.metric << ", ";
    std::cout << "entity_idx:" << select.entity_idx << ", ";
    std::cout << "tag:" << select.tag ;
    std::cout << std::endl;

    char buffer[2048];
    snprintf(buffer, sizeof(buffer),
                 "    summary: timestamp:%ld, tag:%s, count:%d, sum:%ld, avg:%d, min:%d, max:%d, p10:%d, p50:%d, p90:%d",
             select.timestamp, select.summary.tag.c_str(), select.summary.count, select.summary.value_sum,
             select.summary.value_avg, select.summary.value_min, select.summary.value_max, select.summary.value_p10,
             select.summary.value_p50, select.summary.value_p90);
    std::cout << buffer << std::endl;
    
    std::cout << "detail:" << std::endl;
    for (size_t i=0; i<select.info.size(); ++i){
        std::cout << i << "=>" << std::endl;
        snprintf(buffer, sizeof(buffer),
                 "    tag:%s, count:%d, sum:%ld, avg:%d, min:%d, max:%d, p10:%d, p50:%d, p90:%d",
                 select.info[i].tag.c_str(), select.info[i].count,   select.info[i].value_sum,
                 select.info[i].value_avg, select.info[i].value_min, select.info[i].value_max,
                 select.info[i].value_p10, select.info[i].value_p50, select.info[i].value_p90);
        std::cout << buffer << std::endl;
    }
    
    std::cout << "EOF" << std::endl;
    return 0;
}

static int select_by_tm(const std::string& service, const std::string& metric, int64_t tm, int64_t tm_start) {

    event_cond_t cond {};
    cond.tm_start = tm_start;
    cond.tm_interval = tm;
    cond.service = service;
    cond.metric  = metric;
    cond.groupby = GroupType::kGroupbyTimestamp;

    event_select_t select {};

    int code = selecter_->select_stat(cond, select);
    if( code != 0) {
        std::cout << "select fail for " << service << ", " << metric << std::endl;
        return -1;
    }

    std::cout << "select_stat_by_tag info:" << std::endl;

    std::cout << "select condition return:" << std::endl;
    std::cout << "    timestamp:" << select.timestamp << ", ";
    std::cout << "tm_interval:" << select.tm_interval << ", ";
    std::cout << "service:" << select.service << ", ";
    std::cout << "metric:" << select.metric << ", ";
    std::cout << "entity_idx:" << select.entity_idx << ", ";
    std::cout << "tag:" << select.tag ;
    std::cout << std::endl;

    char buffer[2048];
    snprintf(buffer, sizeof(buffer),
                 "    summary: timestamp:%ld, tag:%s, count:%d, sum:%ld, avg:%d, min:%d, max:%d, p10:%d, p50:%d, p90:%d",
             select.timestamp, select.summary.tag.c_str(), select.summary.count, select.summary.value_sum,
             select.summary.value_avg, select.summary.value_min, select.summary.value_max, select.summary.value_p10,
             select.summary.value_p50, select.summary.value_p90);
    std::cout << buffer << std::endl;

    std::cout << "detail:" << std::endl;
    for (size_t i=0; i<select.info.size(); ++i){
        std::cout << i << "=>" << std::endl;
        snprintf(buffer, sizeof(buffer),
                 "    tag:%s, count:%d, sum:%ld, avg:%d, min:%d, max:%d, p10:%d, p50:%d, p90:%d",
                 select.info[i].tag.c_str(), select.info[i].count,   select.info[i].value_sum,
                 select.info[i].value_avg, select.info[i].value_min, select.info[i].value_max,
                 select.info[i].value_p10, select.info[i].value_p50, select.info[i].value_p90);
        std::cout << buffer << std::endl;
    }

    std::cout << "EOF" << std::endl;

    return 0;
}

static int select_by_tag_order(const std::string& service, const std::string& metric, int64_t tm, int64_t tm_start) {

    event_cond_t cond {};
    cond.tm_start = tm_start;
    cond.tm_interval = tm;
    cond.service = service;
    cond.metric  = metric;
    cond.groupby = GroupType::kGroupbyTag;
    cond.orderby = OrderByType::kOrderByCount;
    cond.orders  = OrderType::kOrderDesc;

    event_select_t select {};

    int code = selecter_->select_stat(cond, select);
    if( code != 0) {
        std::cout << "select fail for " << service << ", " << metric << std::endl;
        return -1;
    }

    std::cout << "select_stat_by_tag info:" << std::endl;

    std::cout << "select condition return:" << std::endl;
    std::cout << "    timestamp:" << select.timestamp << ", ";
    std::cout << "tm_interval:" << select.tm_interval << ", ";
    std::cout << "service:" << select.service << ", ";
    std::cout << "metric:" << select.metric << ", ";
    std::cout << "entity_idx:" << select.entity_idx << ", ";
    std::cout << "tag:" << select.tag ;
    std::cout << std::endl;

    char buffer[2048];
    snprintf(buffer, sizeof(buffer),
                 "    summary: timestamp:%ld, tag:%s, count:%d, sum:%ld, avg:%d, min:%d, max:%d, p10:%d, p50:%d, p90:%d",
             select.timestamp, select.summary.tag.c_str(), select.summary.count, select.summary.value_sum,
             select.summary.value_avg, select.summary.value_min, select.summary.value_max, select.summary.value_p10,
             select.summary.value_p50, select.summary.value_p90);
    std::cout << buffer << std::endl;

    std::cout << "detail:" << std::endl;
    for (size_t i=0; i<select.info.size(); ++i){
        std::cout << i << "=>" << std::endl;
        snprintf(buffer, sizeof(buffer),
                 "    tag:%s, count:%d, sum:%ld, avg:%d, min:%d, max:%d, p10:%d, p50:%d, p90:%d",
                 select.info[i].tag.c_str(), select.info[i].count,   select.info[i].value_sum,
                 select.info[i].value_avg, select.info[i].value_min, select.info[i].value_max,
                 select.info[i].value_p10, select.info[i].value_p50, select.info[i].value_p90);
        std::cout << buffer << std::endl;
    }

    std::cout << "EOF" << std::endl;

    return 0;
}

static int select_by_tm_order(const std::string& service, const std::string& metric, int64_t tm, int64_t tm_start) {

    event_cond_t cond {};
    cond.tm_start = tm_start;
    cond.tm_interval = tm;
    cond.service = service;
    cond.metric  = metric;
    cond.groupby = GroupType::kGroupbyTimestamp;
    cond.orderby = OrderByType::kOrderByCount;
    cond.orders  = OrderType::kOrderDesc;

    event_select_t select {};

    int code = selecter_->select_stat(cond, select);
    if( code != 0) {
        std::cout << "select fail for " << service << ", " << metric << std::endl;
        return -1;
    }

    std::cout << "select_stat_by_tag info:" << std::endl;

    std::cout << "select condition return:" << std::endl;
    std::cout << "    timestamp:" << select.timestamp << ", ";
    std::cout << "tm_interval:" << select.tm_interval << ", ";
    std::cout << "service:" << select.service << ", ";
    std::cout << "metric:" << select.metric << ", ";
    std::cout << "entity_idx:" << select.entity_idx << ", ";
    std::cout << "tag:" << select.tag ;
    std::cout << std::endl;

    char buffer[2048];
    snprintf(buffer, sizeof(buffer),
                 "    summary: timestamp:%ld, tag:%s, count:%d, sum:%ld, avg:%d, min:%d, max:%d, p10:%d, p50:%d, p90:%d",
             select.timestamp, select.summary.tag.c_str(), select.summary.count, select.summary.value_sum,
             select.summary.value_avg, select.summary.value_min, select.summary.value_max, select.summary.value_p10,
             select.summary.value_p50, select.summary.value_p90);
    std::cout << buffer << std::endl;

    std::cout << "detail:" << std::endl;
    for (size_t i=0; i<select.info.size(); ++i){
        std::cout << i << "=>" << std::endl;
        snprintf(buffer, sizeof(buffer),
                 "    tag:%s, count:%d, sum:%ld, avg:%d, min:%d, max:%d, p10:%d, p50:%d, p90:%d",
                 select.info[i].tag.c_str(), select.info[i].count,   select.info[i].value_sum,
                 select.info[i].value_avg, select.info[i].value_min, select.info[i].value_max,
                 select.info[i].value_p10, select.info[i].value_p50, select.info[i].value_p90);
        std::cout << buffer << std::endl;
    }

    std::cout << "EOF" << std::endl;

    return 0;
}

using namespace heracles_client;

// ./select_detail subcmd service metric tm(s)
// sub_cmd => tag, tm, tag_order, tm_order
int main(int argc, char* argv[]) {

    std::string addr_ip = "127.0.0.1";
    uint16_t    addr_port = 8435;


    selecter_ = std::make_shared<heracles_client::HeraclesClient>();
    if (!selecter_ || !selecter_ ->init(addr_ip, addr_port, ::syslog)) {
        std::cout << "init client failed." << std::endl;
        return -1;
    }

    // http://tool.chinaz.com/Tools/unixtime.aspx
    if (argc != 6) {
        std::cout << " ./select_detail subcmd service metric tm_interval(s) tm_start(2019-04-23 15:50:46)" << std::endl;
        std::cout << " supported subcmd: tag, tm, tag_order, tm_order" << std::endl;
        return -1;
    }

    std::string subcmd  = argv[1];
    std::string service = argv[2];
    std::string metric  = argv[3];
    int64_t     tm      = ::atoll(argv[4]);
    int64_t     tm_start= to_unixstamp(argv[5]);
    
    if (subcmd == "tag") {
        select_by_tag(service, metric, tm, tm_start);
    } else if (subcmd == "tm") {
        select_by_tm(service, metric, tm, tm_start);
    } else if (subcmd == "tag_order") {
        select_by_tag_order(service, metric, tm, tm_start);
    } else if (subcmd == "tm_order") {
        select_by_tm_order(service, metric, tm, tm_start);
    } else {
        std::cout << "unknown subcmd: " << subcmd << std::endl;
    }

    return 0;
}
