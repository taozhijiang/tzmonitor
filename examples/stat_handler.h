/*-
 * Copyright (c) 2019 TAO Zhijiang<taozhijiang@gmail.com>
 *
 * Licensed under the BSD-3-Clause license, see LICENSE for full information.
 *
 */


#ifndef _EXAMPLES_STAT_HANDLER_H_
#define _EXAMPLES_STAT_HANDLER_H_

#include <sstream>
#include <tzhttpd/HttpParser.h>

#include <Client/include/MonitorClient.h>

namespace tzhttpd {

namespace http_handler {

// init only once at startup
extern std::string http_server_version;
static std::string null_string = "";


int index_http_get_handler(const HttpParser& http_parser,
                           std::string& response, string& status_line, std::vector<std::string>& add_header );
int stats_http_get_handler(const HttpParser& http_parser,
                           std::string& response, string& status_line, std::vector<std::string>& add_header);

class StatHandler {

public:

    explicit StatHandler(const HttpParser& http_parser, const std::string& post_data = null_string):
        http_parser_(http_parser) {
    }

    int fetch_stat(std::string& str) {

        struct timeval start;
        ::gettimeofday(&start, NULL);

        ss_ << "<html>" << std::endl;
        ss_ << "<META http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\">"<< std::endl;
        ss_ << "<head>" << std::endl;

        ss_ <<  "<style> "
                "table { "
                "   font-family:\"Trebuchet MS\", Arial, Helvetica, sans-serif; "
                "    border-collapse: collapse; "
                "    table-layout: fixed; } "
                "th, td { "
                "    text-align: left; "
                "    padding: 3px; }"
                "tr:nth-child(even){background-color: #F2F2F2;} "
                "tr:nth-child(odd) {background-color: #EAF2D3;} "
                "</style>"
        << std::endl;
        ss_ << "</head>" << std::endl;

        ss_ << "<body align=center>" << std::endl;

        print_menu();

        ss_ << "<table align=center>" << std::endl;

        print_head();
        if(print_items() != 0)
            return -1;

        struct timeval end;
        ::gettimeofday(&end, NULL);

        int64_t time_ms = ( 1000000 * ( end.tv_sec - start.tv_sec ) + end.tv_usec - start.tv_usec ) / 1000; // ms
        int64_t time_us = ( 1000000 * ( end.tv_sec - start.tv_sec ) + end.tv_usec - start.tv_usec ) % 1000; // us

        ss_ << "</table>" << std::endl;

        ss_ << "<p>tzhttpd (" << http_server_version <<") result build in " << time_ms << "." << time_us << "ms.</p>";
        ss_ << "</body>" << std::endl;
        ss_ << "</html>" << std::endl;

        str =  ss_.str();
        return 0;
    }

    virtual ~StatHandler() {
    }

private:
    virtual void print_head() = 0;
    virtual int print_items() = 0;

    virtual void print_menu() {
        ss_ << "<br />" << std::endl;
        ss_ << "<a href=\"/monitor\">返回首页</a>" << std::endl;
        ss_ << "<a href=\"\">刷新本页</a>" << std::endl;
        ss_ << "<br />" << std::endl;
    }

protected:
    std::stringstream ss_;
    const HttpParser& http_parser_;
};


// 定制接口

class IndexStatHandler: public StatHandler {
public:
    explicit IndexStatHandler(const HttpParser& http_parser)
    :StatHandler(http_parser){
    }

private:
    virtual void print_head();
    virtual int print_items();
};

class EventStatHandler: public StatHandler {
public:
    explicit EventStatHandler(const HttpParser& http_parser)
    :StatHandler(http_parser), cond_() {
        cond_.version = "1.0.0";
    }

private:
    virtual void print_head();
    virtual int print_items();

private:
    event_cond_t cond_;
};

} // end namespace http_handler

} // end namespace tzhttpd


#endif //_EXAMPLES_HTTP_HANDLER_H_
