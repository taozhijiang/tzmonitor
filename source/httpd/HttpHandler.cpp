#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <fstream>
#include <sstream>
#include <boost/algorithm/string.hpp>

#include <json/json.h>

#include "ErrorDef.h"
#include "HttpProto.h"
#include "HttpHandler.h"
#include "StatHandler.h"
#include "HttpServer.h"

#include "Helper.h"
#include <utils/Utils.h>
#include <utils/Log.h>

#include <core/EventItem.h>
#include <core/EventRepos.h>

namespace http_handler {

using namespace http_proto;

static bool check_and_sendfile(const HttpParser& http_parser, std::string regular_file_path,
                                   std::string& response, string& status_line) {

    // check dest is directory or regular?
    struct stat sb;
    if (stat(regular_file_path.c_str(), &sb) == -1) {
        log_err("Stat file error: %s", regular_file_path.c_str());
        response = http_proto::content_error;
        status_line = generate_response_status_line(http_parser.get_version(), StatusCode::server_error_internal_server_error);
        return false;
    }

    if (sb.st_size > 100*1024*1024 /*100M*/) {
        log_err("Too big file size: %ld", sb.st_size);
        response = http_proto::content_bad_request;
        status_line = generate_response_status_line(http_parser.get_version(), StatusCode::client_error_bad_request);
        return false;
    }

    std::ifstream fin(regular_file_path);
    fin.seekg(0);
    std::stringstream buffer;
    buffer << fin.rdbuf();
    response = buffer.str();
    status_line = generate_response_status_line(http_parser.get_version(), StatusCode::success_ok);

    return true;
}


int default_http_get_handler(const HttpParser& http_parser, std::string& response, string& status_line) {

    const UriParamContainer& params = http_parser.get_request_uri_params();
    if (!params.EMPTY()) {
        log_err("Default handler just for static file transmit, we can not handler uri parameters...");
    }

    std::string real_file_path = helper::request_http_docu_root() + "/" + http_parser.find_request_header(http_proto::header_options::request_path_info);

    // check dest exist?
    if (::access(real_file_path.c_str(), R_OK) != 0) {
        log_err("File not found: %s", real_file_path.c_str());
        response = http_proto::content_not_found;
        status_line = generate_response_status_line(http_parser.get_version(), StatusCode::client_error_not_found);
        return -1;
    }

    // check dest is directory or regular?
    struct stat sb;
    if (stat(real_file_path.c_str(), &sb) == -1) {
        log_err("Stat file error: %s", real_file_path.c_str());
        response = http_proto::content_error;
        status_line = generate_response_status_line(http_parser.get_version(), StatusCode::server_error_internal_server_error);
        return -1;
    }

    switch (sb.st_mode & S_IFMT) {
        case S_IFREG:
            check_and_sendfile(http_parser, real_file_path, response, status_line);
            break;

        case S_IFDIR:
            {
                bool OK = false;
                const std::vector<std::string> &indexes = helper::request_http_docu_index();
                for (std::vector<std::string>::const_iterator iter = indexes.cbegin(); iter != indexes.cend(); ++iter) {
                    std::string file_path = real_file_path + "/" + *iter;
                    log_info("Trying: %s", file_path.c_str());
                    if (check_and_sendfile(http_parser, file_path, response, status_line)) {
                        OK = true;
                        break;
                    }
                }

                if (!OK) {
                    // default, 404
                    response = http_proto::content_not_found;
                    status_line = generate_response_status_line(http_parser.get_version(), StatusCode::success_ok);
                }
            }
            break;

        default:
            break;
    }

    return 0;
}


int get_ev_query_handler(const HttpParser& http_parser, std::string& response, string& status_line) {

    do {

        event_cond_t cond {};
        std::string value;

        // required
        if (http_parser.get_request_uri_param("version", value)) {
            cond.version = value;
        }

        if (http_parser.get_request_uri_param("name", value)) {
            cond.name = value;
        }

        if (http_parser.get_request_uri_param("interval_sec", value)) {
            cond.interval_sec = ::atoll(value.c_str());
        }

        if (cond.version.empty() || cond.name.empty() || cond.interval_sec <= 0) {
            log_err("required param missing...");
            break;
        }

        // optional
        if (http_parser.get_request_uri_param("start", value)) {
            cond.start = ::atoll(value.c_str());
        }

        if (http_parser.get_request_uri_param("host", value)) {
            cond.host = value;
        }

        if (http_parser.get_request_uri_param("serv", value)) {
            cond.serv = value;
        }

        if (http_parser.get_request_uri_param("entity_idx", value)) {
            cond.entity_idx = value;
        }

        if (http_parser.get_request_uri_param("flag", value)) {
            cond.flag = value;
        }

        cond.groupby = GroupType::kGroupNone;
        if (http_parser.get_request_uri_param("groupby", value)) {
            if (boost::iequals(value, "time")) {
                cond.groupby = GroupType::kGroupbyTime;
            } else if (boost::iequals(value, "flag")) {
                cond.groupby = GroupType::kGroupbyFlag;
            }
        }

        event_query_t stat{};
        if (EventRepos::instance().get_event(cond, stat) != ErrorDef::OK) {
            log_err("call get_event detail failed!");
            break;
        }

         // build request json
        Json::Value root;
        root["version"] = "1.0.0";
        root["name"] = cond.name;
        root["time"] = convert_to_string(stat.time);
        root["interval_sec"] = convert_to_string(cond.interval_sec);

        if(!cond.host.empty()) root["host"] = cond.host;
        if(!cond.serv.empty()) root["serv"] = cond.serv;
        if(!cond.entity_idx.empty()) root["entity_idx"] = cond.entity_idx;
        if(!cond.flag.empty()) root["flag"] = cond.flag;

        Json::Value summary;
        Json::FastWriter fast_writer;

        summary["count"] = convert_to_string(stat.summary.count);
        summary["value_sum"] = convert_to_string(stat.summary.value_sum);
        summary["value_avg"] = convert_to_string(stat.summary.value_avg);
        summary["value_std"] = convert_to_string(stat.summary.value_std);
        root["summary"] = fast_writer.write(summary);

        if (cond.groupby != GroupType::kGroupNone) {
            Json::Value ordersJson;
            for (auto iter = stat.info.begin(); iter != stat.info.end(); ++iter) {
                Json::Value orderjson{};

                if (cond.groupby == GroupType::kGroupbyTime) {
                    orderjson["time"] = convert_to_string(iter->time);
                } else if (cond.groupby == GroupType::kGroupbyFlag) {
                    orderjson["flag"] = convert_to_string(iter->flag);
                }

                orderjson["count"] = convert_to_string(iter->count);
                orderjson["value_sum"] = convert_to_string(iter->value_sum);
                orderjson["value_avg"] = convert_to_string(iter->value_avg);
                orderjson["value_std"] = convert_to_string(iter->value_std);

                ordersJson.append(orderjson);
            }

            root["info"] = fast_writer.write(ordersJson);
        }

        response = fast_writer.write(root);
        status_line = generate_response_status_line(http_parser.get_version(), StatusCode::success_ok);

        return ErrorDef::OK;

    } while (0);

    response = http_proto::content_error;
    status_line = generate_response_status_line(http_parser.get_version(), StatusCode::server_error_internal_server_error);
    return ErrorDef::Error;
}


int post_ev_submit_handler(const HttpParser& http_parser, const std::string& post_data, std::string& response, string& status_line) {

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
            status_line = generate_response_status_line(http_parser.get_version(), StatusCode::success_ok);
            return ErrorDef::OK;
        }

    } while (0);

    response = http_proto::content_error;
    status_line = generate_response_status_line(http_parser.get_version(), StatusCode::server_error_internal_server_error);
    return ErrorDef::Error;
}


int index_http_get_handler(const HttpParser& http_parser, std::string& response, string& status_line) {
    std::unique_ptr<IndexStatHandler> handler(new IndexStatHandler(http_parser));
    if (handler->fetch_stat(response) == 0) {
        status_line = generate_response_status_line(http_parser.get_version(), StatusCode::success_ok);
        return 0;
    }

    response = http_proto::content_error;
    status_line = generate_response_status_line(http_parser.get_version(), StatusCode::server_error_internal_server_error);
    return -1;
}

int event_stat_http_get_handler(const HttpParser& http_parser, std::string& response, string& status_line) {

    std::unique_ptr<EventStatHandler> handler(new EventStatHandler(http_parser));
    if (handler->fetch_stat(response) == 0) {
        status_line = generate_response_status_line(http_parser.get_version(), StatusCode::success_ok);
        return 0;
    }

    response = http_proto::content_error;
    status_line = generate_response_status_line(http_parser.get_version(), StatusCode::server_error_internal_server_error);
    return -1;
}

int get_test_handler(const HttpParser& http_parser, std::string& response, string& status_line) {

    response = http_proto::content_ok;
    status_line = generate_response_status_line(http_parser.get_version(), StatusCode::success_ok);

    return 0;
}


int post_test_handler(const HttpParser& http_parser, const std::string& post_data, std::string& response, string& status_line) {

    response = post_data;
    status_line = generate_response_status_line(http_parser.get_version(), StatusCode::success_ok);

    return 0;
}

} // end namespace

