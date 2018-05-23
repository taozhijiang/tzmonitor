#include <algorithm>
#include <vector>
#include <string>
#include <sstream>
#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>

#include <utils/Utils.h>
#include <connect/SqlConn.h>
#include "Helper.h"

#include <core/EventRepos.h>
#include <core/EventSql.h>
#include "StatHandler.h"


namespace http_handler {

// paybank submitstat

void IndexStatHandler::print_head() override {

    ss_ << "<h3 align=\"center\">" << "TZMonitir监控系统使用手册" << "</h2>" << std::endl;

    ss_ << "<tr style=\"font-weight:bold; font-style:italic;\">" << std::endl;

    ss_ << "<td>" << "序号" << "</td>" << std::endl;
    ss_ << "<td>" << "访问 A" << "</td>" << std::endl;
    ss_ << "<td>" << "访问 B" << "</td>" << std::endl;
    ss_ << "<td>" << "访问 C" << "</td>" << std::endl;
    ss_ << "<td>" << "访问 D" << "</td>" << std::endl;
    ss_ << "<td>" << "访问 E" << "</td>" << std::endl;

    ss_ << "</tr>" << std::endl;
}

int IndexStatHandler::print_items() override {

    std::vector<std::string> distinct_name;

    if (EventSql::get_distinct_ev_name(distinct_name) != ErrorDef::OK) {
        log_err("query distinct_name failed!");
        return -1;
    }

    if (distinct_name.empty()) {
        log_info("Database may empty!");
        ss_ << "<tr>" << std::endl;
        ss_ << "<td>" << "EMPTY" << "</td>" << std::endl;
        ss_ << "</tr>" << std::endl;

        return 0;
    }

    for (size_t i=0; i<distinct_name.size(); ++i) {

        const std::string& name = distinct_name[i];

        ss_ << "<tr>" << std::endl;
        ss_ << "<td>" << i << ". " << name << "</td>" << std::endl;
        ss_ << va_format("<td><a href=\"/stat?name=%s&interval_sec=60\">1min</a></td>",    name.c_str()) << std::endl;
        ss_ << va_format("<td><a href=\"/stat?name=%s&interval_sec=600\">10min</a></td>",  name.c_str()) << std::endl;
        ss_ << va_format("<td><a href=\"/stat?name=%s&interval_sec=1800\">30min</a></td>", name.c_str()) << std::endl;
        ss_ << va_format("<td><a href=\"/stat?name=%s&interval_sec=3600\">1hour</a></td>", name.c_str()) << std::endl;
        ss_ << va_format("<td><a href=\"/stat?name=%s&interval_sec=86400\">1day</a></td>", name.c_str()) << std::endl;
        ss_ << "</tr>" << std::endl;
    }

    return 0;
}


static inline std::string time_to_date(const time_t& tt) {
    tm* t= localtime(&tt);
    char time_buf[128] = {0,};
    sprintf(time_buf,"%d-%02d-%02d", t->tm_year + 1900, t->tm_mon + 1, t->tm_mday);
    return time_buf;
}

static inline std::string time_to_datetime(const time_t& tt) {
    if (tt == 0) {
        return "";
    }

    tm *t = localtime(&tt);
    char time_buf[128] = {0,};
    sprintf(time_buf,"%d-%02d-%02d %02d:%02d:%02d", t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
                                                    t->tm_hour, t->tm_min, t->tm_sec);
    return time_buf;
}

// all kinds stat

void EventStatHandler::print_head() override {

    if (!http_parser_.get_request_uri_param("name", cond_.name)) {
        log_err("required param name not found.");
        return;
    }

    std::string time;
    cond_.interval_sec = 60; // default 1min
    if (http_parser_.get_request_uri_param("interval_sec", time)) {
        time_t n_time = ::atoll(time.c_str());
        if (n_time > 0) {
            cond_.interval_sec = n_time;
        }
    }

    log_debug("query %s - %ld status.", cond_.name.c_str(), cond_.interval_sec);

    ss_ << "<h3 align=\"center\">" << "监测事件名称：" << cond_.name<< ", 时长："
        << cond_.interval_sec << " sec </h2>" << std::endl;

    ss_ << "<tr style=\"font-weight:bold; font-style:italic;\">" << std::endl;

    ss_ << "<td></td>" << std::endl;
    ss_ << "<td>" << "F_idx" << "</td>" << std::endl;
    ss_ << "<td>" << "F_time" << "</td>" << std::endl;
    ss_ << "<td>" << "F_flag" << "</td>" << std::endl;
    ss_ << "<td>" << "F_count" << "</td>" << std::endl;
    ss_ << "<td>" << "F_value_sum" << "</td>" << std::endl;
    ss_ << "<td>" << "F_value_avg" << "</td>" << std::endl;
    ss_ << "<td>" << "F_value_std" << "</td>" << std::endl;

    ss_ << "</tr>" << std::endl;
}

static std::string build_record(size_t idx, const event_info_t& info) {
    std::stringstream ss;

    ss << "<tr>" << std::endl;
    ss << "<td></td>" << std::endl;
    ss << "<td>" << idx << "</td>" << std::endl;
    ss << "<td>" << time_to_datetime(info.time) << "</td>" << std::endl;
    ss << "<td>" << info.flag << "</td>" << std::endl;
    ss << "<td>" << info.count << "</td>" << std::endl;
    ss << "<td>" << info.value_sum << "</td>" << std::endl;
    ss << "<td>" << info.value_avg << "</td>" << std::endl;
    ss << "<td>" << info.value_std << "</td>" << std::endl;
    ss << "<tr>" << std::endl;

    return ss.str();
}

int EventStatHandler::print_items() override {

    size_t idx = 0;
    cond_.groupby = GroupType::kGroupbyFlag;
    event_query_t result {};

    if (EventRepos::instance().get_event(cond_, result) != ErrorDef::OK) {
        log_err("query failed!");
        return ErrorDef::Error;
    }

    ss_ << "<tr style=\"font-weight:bold;\">" << std::endl;
    ss_ << "<td>" << "[统计总览]" << "</td>" << std::endl;
    ss_ << build_record(0, result.summary);
    ss_ << "</tr>" << std::endl;


    ss_ << "<tr style=\"font-weight:bold;\">" << std::endl;
    ss_ << "<td>" << "[按标签分类明细]" << "</td>" << std::endl;
    for (idx = 0; idx < result.info.size(); ++idx) {
        ss_ << build_record(idx, result.info[idx]);
    }
    ss_ << "</tr>" << std::endl;


    // 10min明细可以接受
    if (cond_.interval_sec <= 600) {

        cond_.groupby = GroupType::kGroupbyTime;
        if (EventRepos::instance().get_event(cond_, result) != ErrorDef::OK) {
            log_err("query failed!");
            return ErrorDef::Error;
        }

        ss_ << "<tr style=\"font-weight:bold;\">" << std::endl;
        ss_ << "<td>" << "[按时间分类明细]" << "</td>" << std::endl;
        for (idx = 0; idx < result.info.size(); ++idx) {
            ss_ << build_record(idx, result.info[idx]);
        }
        ss_ << "</tr>" << std::endl;
    }

    return ErrorDef::OK;
}

} // end namespace

