/*-
 * Copyright (c) 2019 TAO Zhijiang<taozhijiang@gmail.com>
 *
 * Licensed under the BSD-3-Clause license, see LICENSE for full information.
 *
 */

#include <syslog.h>

#include <algorithm>
#include <vector>
#include <string>
#include <sstream>
#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>

#include <tzhttpd/HttpProto.h>
#include <tzhttpd/Log.h>

#include "stat_handler.h"

#include <Client/include/MonitorClient.h>

namespace tzhttpd {

namespace http_handler {


static const int kMaxBuffSize = 2*8190;
static std::string str_format(const char * fmt, ...) {

    char buff[kMaxBuffSize + 1] = {0, };
    uint32_t n = 0;

    va_list ap;
    va_start(ap, fmt);
    n += vsnprintf(buff, kMaxBuffSize, fmt, ap);
    va_end(ap);
    buff[n] = '\0';

    return std::string(buff, n);
}


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

int stats_http_get_handler(const HttpParser& http_parser,
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
    ss_ << "<td>" << "访问 F" << "</td>" << std::endl;

    ss_ << "</tr>" << std::endl;
}

int IndexStatHandler::print_items() override {

    auto reporter = std::make_shared<tzmonitor_client::MonitorClient>();
    if (!reporter || !reporter ->init("tzmonitor.conf", ::syslog)) {
        tzhttpd_log_err("init client failed.");
        return -1;
    }

    if (reporter->ping()) {
        tzhttpd_log_err("client call ping failed.");
        return -1;
    }
    
    std::vector<std::string> services;
    if (reporter->known_services(services) != 0) {
        tzhttpd_log_err("client call known_services failed.");
        return -1;
    }
    
    std::map<std::string, std::vector<std::string>> service_metrics;
    std::map<std::string, event_handler_conf_t> service_handler_confs;

    for (size_t i=0; i<services.size(); ++i) {
        event_handler_conf_t handler_conf {};
        std::vector<std::string> metrics;
        if (reporter->known_metrics(handler_conf, metrics, services[i]) != 0) {
            tzhttpd_log_err("client call known_metrics failed.");
            continue;
        }
        
        if(!metrics.empty()) {
            service_metrics[services[i]] = metrics;
            service_handler_confs[services[i]] = handler_conf;
        }
    }

    
    for (size_t i=0; i<services.size(); ++i) {
        
        const std::string& name = services[i];
        const std::vector<std::string> metrics = service_metrics[name];

        char buff[128] {};
        snprintf(buff, sizeof(buff), "st:%d,lg:%d,sr:%s", service_handler_confs[name].event_step_,
                 service_handler_confs[name].event_linger_, service_handler_confs[name].store_type_.c_str());

        ss_ << "<tr><td>" << i << ". " << name << "</td></tr>" << std::endl;
        ss_ << "<tr><td>" << buff << "</td></tr>" << std::endl;

        for(size_t j=0; j<metrics.size(); ++j) {
            ss_ << "<tr>" << std::endl;
            ss_ << "<td> </td>" << std::endl;
            ss_ << str_format("<td>\t<a href=\"/monitor/stats?version=1.0.0&service=%s&metric=%s&tm_interval=60\">%s[by 1min]</a></td>",
                              name.c_str(), metrics[j].c_str(), metrics[j].c_str()) << std::endl;
            ss_ << str_format("<td>\t<a href=\"/monitor/stats?version=1.0.0&service=%s&metric=%s&tm_interval=120\">%s[by 2min]</a></td>",
                              name.c_str(), metrics[j].c_str(), metrics[j].c_str()) << std::endl;
            ss_ << str_format("<td>\t<a href=\"/monitor/stats?version=1.0.0&service=%s&metric=%s&tm_interval=600\">%s[by 10min]</a></td>",
                              name.c_str(), metrics[j].c_str(), metrics[j].c_str()) << std::endl;
            ss_ << str_format("<td>\t<a href=\"/monitor/stats?version=1.0.0&service=%s&metric=%s&tm_interval=1800\">%s[by 30min]</a></td>",
                              name.c_str(), metrics[j].c_str(), metrics[j].c_str()) << std::endl;
            ss_ << str_format("<td>\t<a href=\"/monitor/stats?version=1.0.0&service=%s&metric=%s&tm_interval=3600\">%s[by 1hour]</a></td>",
                              name.c_str(), metrics[j].c_str(), metrics[j].c_str()) << std::endl;
            ss_ << str_format("<td>\t<a href=\"/monitor/stats?version=1.0.0&service=%s&metric=%s&tm_interval=86400\">%s[by 1day]</a></td>",
                              name.c_str(), metrics[j].c_str(), metrics[j].c_str()) << std::endl;
            ss_ << "</tr>" << std::endl;
        }
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

    if (http_parser_.get_request_uri_param("service", value)) {
        cond_.service = value;
    }

    if (http_parser_.get_request_uri_param("metric", value)) {
        cond_.metric = value;
    }

    cond_.tm_interval = 60; // default 1min
    if (http_parser_.get_request_uri_param("tm_interval", value)) {
        cond_.tm_interval = ::atoll(value.c_str());
        if (cond_.tm_interval <= 0) {
            cond_.tm_interval = 60; // default 1min
        }
    }

    if (cond_.version.empty() || cond_.service.empty() ||
        cond_.metric.empty() || cond_.tm_interval <= 0) {
        tzhttpd_log_err("required param missing...");
        ss_ << "<h3 align=\"center\">Param Error</h3>";
        return;
    }

    ss_ << "<h3 align=\"center\">"
        << "服务：" << cond_.service
        << ", 事件名称：" << cond_.metric
        << ", 时长：" << cond_.tm_interval << " sec" << std::endl;

    // optional
    if (http_parser_.get_request_uri_param("tm_start", value)) {
        cond_.tm_start = ::atoll(value.c_str());
        ss_ << ", 开始时间：" << cond_.tm_start;
    }

    if (http_parser_.get_request_uri_param("entity_idx", value)) {
        cond_.entity_idx = value;
        ss_ << ", 服务Idx：" << cond_.entity_idx;
    }

    if (http_parser_.get_request_uri_param("tag", value)) {
        cond_.tag = value;
        ss_ << ", Tag：" << cond_.tag;
    }

    ss_ << "</h3>" << std::endl;

    ss_ << "<tr style=\"font-weight:bold; font-style:italic;\">" << std::endl;

    ss_ << "<td></td>" << std::endl;
    ss_ << "<td>" << "F_idx" << "</td>" << std::endl;
    ss_ << "<td>" << "F_timestamp" << "</td>" << std::endl;
    ss_ << "<td>" << "F_tag" << "</td>" << std::endl;
    ss_ << "<td>" << "F_count" << "</td>" << std::endl;
    ss_ << "<td>" << "F_value_sum" << "</td>" << std::endl;
    ss_ << "<td>" << "F_value_avg" << "</td>" << std::endl;
    ss_ << "<td>" << "F_value_min" << "</td>" << std::endl;
    ss_ << "<td>" << "F_value_max" << "</td>" << std::endl;
    ss_ << "<td>" << "F_value_p10" << "</td>" << std::endl;
    ss_ << "<td>" << "F_value_p50" << "</td>" << std::endl;
    ss_ << "<td>" << "F_value_p90" << "</td>" << std::endl;

    ss_ << "</tr>" << std::endl;
}

static std::string build_record(size_t idx, const event_info_t& info) {
    std::stringstream ss;

    ss << "<tr>" << std::endl;
    ss << "<td></td>" << std::endl;
    ss << "<td>" << idx << "</td>" << std::endl;
    if (info.timestamp == 0) {
        ss << "<td> - </td>" << std::endl;
    } else {
        ss << "<td>" << time_to_datetime(info.timestamp) << "</td>" << std::endl;
    }

    if (info.tag.empty()) {
        ss << "<td> - </td>" << std::endl;
    } else {
        ss << "<td>" << info.tag << "</td>" << std::endl;
    }
    ss << "<td>" << info.count << "</td>" << std::endl;
    ss << "<td>" << info.value_sum << "</td>" << std::endl;
    ss << "<td>" << info.value_avg << "</td>" << std::endl;
    ss << "<td>" << info.value_min << "</td>" << std::endl;
    ss << "<td>" << info.value_max << "</td>" << std::endl;
    ss << "<td>" << info.value_p10 << "</td>" << std::endl;
    ss << "<td>" << info.value_p50 << "</td>" << std::endl;
    ss << "<td>" << info.value_p90 << "</td>" << std::endl;
    ss << "<tr>" << std::endl;

    return ss.str();
}

int EventStatHandler::print_items() override {

    size_t idx = 0;
    cond_.groupby = GroupType::kGroupbyTag;
    event_select_t result {};

    auto reporter = std::make_shared<tzmonitor_client::MonitorClient>();
    if (!reporter || !reporter ->init("tzmonitor.conf", ::syslog)) {
        tzhttpd_log_err("init client failed.");
        return -1;
    }

    if (reporter->ping()) {
        tzhttpd_log_err("client call ping failed.");
        return -1;
    }

    // 底层接口
    if (reporter->select_stat(cond_, result) != 0) {
        tzhttpd_log_err("client call select_stat_by_tag failed.");
        return -1;
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
    if (cond_.tm_interval <= 600) {

        result.info.clear();
        cond_.groupby = GroupType::kGroupbyTimestamp;
        if (reporter->select_stat(cond_, result) != 0) {
            tzhttpd_log_err("query by timestamp failed!");
            return -1;
        }

        ss_ << "<tr style=\"font-weight:bold;\">" << std::endl;
        ss_ << "<td>" << "[按时间分类明细]" << "</td>" << std::endl;
        for (idx = 0; idx < result.info.size(); ++idx) {
            ss_ << build_record(idx, result.info[idx]);
        }
        ss_ << "</tr>" << std::endl;
    }

    return 0;
}

} // end namespace http_handler
} // end namespace tzhttpd

