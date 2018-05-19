#ifndef _TZ_HTTP_HANDLER_H_
#define _TZ_HTTP_HANDLER_H_

// 所有的http uri 路由

#include "General.h"
#include "HttpParser.h"

typedef boost::function<int (const HttpParser& http_parser, const std::string& post_data, std::string& response, string& status_line)> HttpPostHandler;
typedef boost::function<int (const HttpParser& http_parser, std::string& response, string& status_line)> HttpGetHandler;

namespace http_handler {

int default_http_get_handler(const HttpParser& http_parser, std::string& response, string& status_line);

int get_ev_query_handler(const HttpParser& http_parser, std::string& response, string& status_line);
int post_ev_submit_handler(const HttpParser& http_parser, const std::string& post_data, std::string& response, string& status_line);

// 预留测试接口
int get_test_handler(const HttpParser& http_parser, std::string& response, string& status_line);
int post_test_handler(const HttpParser& http_parser, const std::string& post_data, std::string& response, string& status_line);

} // end namespace


#endif //_TZ_HTTP_HANDLER_H_
