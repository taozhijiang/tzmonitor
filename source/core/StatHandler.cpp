/*-
 * Copyright (c) 2018 TAO Zhijiang<taozhijiang@gmail.com>
 *
 * Licensed under the BSD-3-Clause license, see LICENSE for full information.
 *
 */


#include <algorithm>
#include <vector>
#include <string>
#include <sstream>
#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>

#include <json/json.h>

#include <utils/Utils.h>
#include <connect/SqlConn.h>
#include "Helper.h"

#include <core/EventRepos.h>
#include <core/EventSql.h>

#include <tzhttpd/HttpProto.h>
#include "StatHandler.h"

namespace tzhttpd {

namespace http_handler {

int index_http_get_handler(const HttpParser& http_parser,
                           std::string& response, string& status_line, std::vector<std::string>& add_header) {
    std::unique_ptr<IndexStatHandler> handler(new IndexStatHandler(http_parser));
    if (handler->fetch_stat(response) == 0) {
        status_line = http_proto::generate_response_status_line(http_parser.get_version(),
                                                                http_proto::StatusCode::success_ok);
        add_header = {"Content-Type: text/html; charset=utf-8"};

        return 0;
    }

    response = http_proto::content_error;
    status_line = http_proto::generate_response_status_line(http_parser.get_version(),
                                                            http_proto::StatusCode::server_error_internal_server_error);
    add_header = {"Content-Type: text/html; charset=utf-8"};

    return -1;
}

int event_stat_http_get_handler(const HttpParser& http_parser,
                                std::string& response, string& status_line, std::vector<std::string>& add_header) {

    std::unique_ptr<EventStatHandler> handler(new EventStatHandler(http_parser));
    if (handler->fetch_stat(response) == 0) {
        status_line = http_proto::generate_response_status_line(http_parser.get_version(),
                                                                http_proto::StatusCode::success_ok);
        add_header = {"Content-Type: text/html; charset=utf-8"};

        return 0;
    }

    response = http_proto::content_error;
    status_line = http_proto::generate_response_status_line(http_parser.get_version(),
                                                            http_proto::StatusCode::server_error_internal_server_error);
    add_header = {"Content-Type: text/html; charset=utf-8"};

    return -1;
}

int post_ev_submit_handler(const HttpParser& http_parser, const std::string& post_data,
                           std::string& response, string& status_line, std::vector<std::string>& add_header) {

    Json::Value root;
    Json::Reader reader;

    do {

        if (!reader.parse(post_data, root) || root.isNull()) {
            log_err("parse error for: %s", post_data.c_str());
            break;
        }

        if (!root["version"].isString() || !root["time"].isString() ||
            !root["host"].isString() || !root["serv"].isString() ||
            !root["entity_idx"].isString() || !root["data"].isString()) {
            log_err("param check error: %s", post_data.c_str());
            break;
        }

        Json::Value eventList;
        if (!reader.parse(root["data"].asString(), eventList) || !eventList.isArray()) {
            log_err("parse error for eventlist: %s", root["data"].asString().c_str());
            break;
        }


        event_report_t events {};
        events.version = root["version"].asString();
        events.time = ::atoll(root["time"].asString().c_str());
        events.host = root["host"].asString();
        events.serv = root["serv"].asString();
        events.entity_idx = root["entity_idx"].asString();
        events.data.clear();

        for (int i = 0; i < eventList.size(); i++) {
            if (!eventList[i]["name"].isString() || !eventList[i]["msgid"].isString() ||
                !eventList[i]["value"].isString() || !eventList[i]["flag"].isString()) {
                log_err("event data error!");
                continue;
            }

            event_data_t dat{};
            dat.name = eventList[i]["name"].asString();
            dat.msgid = ::atoll(eventList[i]["msgid"].asString().c_str());
            dat.value = ::atoll(eventList[i]["value"].asString().c_str());
            dat.flag = eventList[i]["flag"].asString();

            events.data.push_back(dat);
        }

        if (EventRepos::instance().add_event(events) == ErrorDef::OK) {
            response = http_proto::content_ok;
            status_line = http_proto::generate_response_status_line(http_parser.get_version(),
                                                                    http_proto::StatusCode::success_ok);
            add_header = {"Content-Type: text/html; charset=utf-8"};

            return ErrorDef::OK;
        }

    } while (0);

    response = http_proto::content_error;
    status_line = http_proto::generate_response_status_line(http_parser.get_version(),
                                                            http_proto::StatusCode::server_error_internal_server_error);
    add_header = {"Content-Type: text/html; charset=utf-8"};

    return ErrorDef::Error;
}

// paybank submitstat

void IndexStatHandler::print_head() override {

    ss_ << "<h3 align=\"center\">" << "tzmonitor监控系统使用手册" << "</h2>" << std::endl;

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
        ss_ << va_format("<td><a href=\"/stat?version=1.0.0&name=%s&interval_sec=60\">1min</a></td>",    name.c_str()) << std::endl;
        ss_ << va_format("<td><a href=\"/stat?version=1.0.0&name=%s&interval_sec=600\">10min</a></td>",  name.c_str()) << std::endl;
        ss_ << va_format("<td><a href=\"/stat?version=1.0.0&name=%s&interval_sec=1800\">30min</a></td>", name.c_str()) << std::endl;
        ss_ << va_format("<td><a href=\"/stat?version=1.0.0&name=%s&interval_sec=3600\">1hour</a></td>", name.c_str()) << std::endl;
        ss_ << va_format("<td><a href=\"/stat?version=1.0.0&name=%s&interval_sec=86400\">1day</a></td>", name.c_str()) << std::endl;
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

    // required
    std::string value;

    if (http_parser_.get_request_uri_param("version", value)) {
        cond_.version = value;
    }

    if (http_parser_.get_request_uri_param("name", value)) {
        cond_.name = value;
    }

    cond_.interval_sec = 60; // default 1min
    if (http_parser_.get_request_uri_param("interval_sec", value)) {
        cond_.interval_sec = ::atoll(value.c_str());
        if (cond_.interval_sec <= 0) {
            cond_.interval_sec = 60; // default 1min
        }
    }

    if (cond_.version.empty() || cond_.name.empty() || cond_.interval_sec <= 0) {
        log_err("required param missing...");
        ss_ << "<h3 align=\"center\">Param Error</h3>";
        return;
    }

    ss_ << "<h3 align=\"center\">" << "监测事件名称：" << cond_.name<< ", 时长："
        << cond_.interval_sec << " sec" << std::endl;

    // optional
    if (http_parser_.get_request_uri_param("start", value)) {
        cond_.start = ::atoll(value.c_str());
        ss_ << ", 开始时间：" << cond_.start;
    }

    if (http_parser_.get_request_uri_param("host", value)) {
        cond_.host = value;
        ss_ << ", 主机：" << cond_.host;
    }

    if (http_parser_.get_request_uri_param("serv", value)) {
        cond_.serv = value;
        ss_ << ", 服务名：" << cond_.serv;
    }

    if (http_parser_.get_request_uri_param("entity_idx", value)) {
        cond_.entity_idx = value;
        ss_ << ", 服务Idx：" << cond_.entity_idx;
    }

    if (http_parser_.get_request_uri_param("flag", value)) {
        cond_.flag = value;
        ss_ << ", Flag：" << cond_.flag;
    }
    ss_ << "</h3>" << std::endl;

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

        result.info.clear();
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

} // end namespace http_handler
} // end namespace tzhttpd

