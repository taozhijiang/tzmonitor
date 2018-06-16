#include <string>
#include <vector>
#include <map>

#include <memory>

#include <boost/noncopyable.hpp>

#include <utils/Log.h>
#include <utils/Utils.h>
#include <utils/HttpUtil.h>
#include <json/json.h>

#include "include/TzMonitor.h"

#include "ErrorDef.h"
#include "EventTypes.h"
#include "TzMonitorHttpClientHelper.h"


class TzMonitorHttpClientHelper::Impl : private boost::noncopyable {
public:
    Impl(const std::string& service_url):
        service_url_(service_url) {
    }

    int http_event_submit(const event_report_t& report) {

        int ret_code = 0;

        do {

            if (report.version.empty() || report.host.empty() ||
                report.serv.empty() || report.entity_idx.empty() ||
                report.time <= 0 )
            {
                log_err("thrift submit param check error!");
                ret_code = ErrorDef::ParamErr;
                break;
            }


            std::map<std::string, std::string> map_param;
            map_param["version"] = report.version;
            map_param["host"] = report.host;
            map_param["serv"] = report.serv;
            map_param["entity_idx"] = report.entity_idx;
            map_param["time"] = convert_to_string(report.time);

            Json::Value ordersJson;
            for (auto iter = report.data.cbegin(); iter != report.data.cend(); ++iter) {
                Json::Value orderjson;
                orderjson["name"] = iter->name;
                orderjson["msgid"] = convert_to_string(iter->msgid);
                orderjson["value"] = convert_to_string(iter->value);
                orderjson["flag"] = iter->flag;

                ordersJson.append(orderjson);
            }
            Json::FastWriter fast_writer;
            map_param["data"] = fast_writer.write(ordersJson);

            // build request json
            Json::Value root;
            for (auto itr = map_param.begin(); itr != map_param.end(); itr++) {
                root[itr->first] = itr->second;
            }
            std::string postData = fast_writer.write(root);

            HttpUtil::HttpClient client;
            std::string sUrl = service_url_ + "/ev_submit";
            if (client.PostByHttp(sUrl, postData) == 0) {
                std::string rdata = client.GetData();
                log_debug("post_submit url: %s, return: %s", sUrl.c_str(), rdata.c_str());
                return ErrorDef::OK;
            }

        } while (0);

        return ret_code;
    }

    int http_event_query(const event_cond_t& cond, event_query_t& resp_info) {

        int ret_code = 0;

        do {

            if (cond.version.empty() || cond.name.empty() || cond.interval_sec <= 0 ) {
                log_err("thrift query param check error!");
                ret_code = ErrorDef::ParamErr;
                break;
            }

            std::map<std::string, std::string> map_params;
            map_params["version"] = cond.version;
            map_params["interval_sec"] = convert_to_string(cond.interval_sec);
            map_params["name"] = cond.name;
            map_params["groupby"] = cond.groupby;
            map_params["host"] = cond.host;
            map_params["serv"] = cond.serv;
            map_params["entity_idx"] = cond.entity_idx;
            map_params["flag"] = cond.flag;
            map_params["start"] = convert_to_string(cond.start);

            std::string sUrl = service_url_ + "/ev_query";
            std::string sCallUrl;
            if (HttpUtil::generate_url(sUrl, map_params, sCallUrl) != 0) {
                log_err("Generate call url failed!");
                ret_code = ErrorDef::Error;
                break;
            }

            HttpUtil::HttpClient client;
            if (client.GetByHttp(sCallUrl) != 0 ) {
                log_err("get http error, url: %s", sCallUrl.c_str());
                ret_code = ErrorDef::HttpErr;
                break;
            }

            std::string rdata = client.GetData();
            log_err("request url: %s, return: %s", sCallUrl.c_str(), rdata.c_str());

            Json::Value root;
            Json::Reader reader;
            if (!reader.parse(rdata, root) || root.isNull()) {
                log_err("parse error: %s", rdata.c_str());
                ret_code = ErrorDef::Error;
                break;
            }

            if (!root["version"].isString() || !root["time"].isString() ||
                !root["name"].isString() || !root["summary"].isString()) {
                log_err("required param is missing.");
                ret_code = ErrorDef::CheckErr;
                break;
            }


            resp_info.version = root["version"].asString();
            resp_info.time = ::atoll(root["time"].asString().c_str());
            resp_info.name = root["name"].asString();
            resp_info.interval_sec = ::atoll(root["interval_sec"].asString().c_str());

            if(root["host"].isString())
                resp_info.host = root["host"].asString();
            if(root["serv"].isString())
                resp_info.serv = root["serv"].asString();
            if(root["entity_idx"].isString())
                resp_info.entity_idx = root["entity_idx"].asString();
            if(root["flag"].isString())
                resp_info.flag = root["flag"].asString();

            if (resp_info.version != cond.version || resp_info.name != cond.name ||
                resp_info.interval_sec != cond.interval_sec ||
                resp_info.host != cond.host || resp_info.serv != cond.serv ||
                resp_info.entity_idx != cond.entity_idx || resp_info.flag != cond.flag )
            {
                log_err("http return does not match request param.");
                ret_code = ErrorDef::CheckErr;
                break;
            }

            Json::Value summary;
            std::string strSummary = root["summary"].asString();
            if (!reader.parse(strSummary, summary) || summary.isNull()) {
                log_err("parse error: %s", strSummary.c_str());
                ret_code = ErrorDef::Error;
                break;
            }

            resp_info.summary.count = ::atoll(summary["count"].asString().c_str());
            resp_info.summary.value_sum = ::atoll(summary["value_sum"].asString().c_str());
            resp_info.summary.value_avg = ::atoll(summary["value_avg"].asString().c_str());
            resp_info.summary.value_std = ::atof(summary["value_std"].asString().c_str());

            if (cond.groupby == GroupType::kGroupNone || !root["info"].isString()) {
                break;
            }

            std::string strInfo = root["info"].asString();
            if(strInfo.length() < 10) {
                log_err("May detail info list is empty: %s", strInfo.c_str());
                break;  // Good, no error;
            }

            Json::Value infoList;
            if (!reader.parse(strInfo, infoList) || !infoList.isArray()) {
                log_err("parse error for: %s", strInfo.c_str());
                ret_code = ErrorDef::Error;
                break;
            }

            std::vector<event_info_t> info;
			
			// jsoncpp的bug，只能使用int或者Json::Value::ArrayIndex，不能使用size_t作为索引
            for (int i = 0; i < infoList.size(); i++) {  
                if (!infoList[i]["count"].isString() || !infoList[i]["value_sum"].isString() ||
                    !infoList[i]["value_avg"].isString() || !infoList[i]["value_std"].isString() )
                {
                    log_err("Check error!");
                    continue;
                }

                event_info_t item;
                if (cond.groupby == GroupType::kGroupbyTime) {
                    item.time = ::atoll(infoList[i]["time"].asString().c_str());
                } else if (cond.groupby == GroupType::kGroupbyFlag) {
                    item.flag = infoList[i]["flag"].asString();
                }

                item.count = ::atoll(infoList[i]["count"].asString().c_str());
                item.value_sum = ::atoll(infoList[i]["value_sum"].asString().c_str());
                item.value_avg = ::atoll(infoList[i]["value_avg"].asString().c_str());
                item.value_std = ::atof(infoList[i]["value_std"].asString().c_str());

                info.push_back(item);

            }

            // collect it
            resp_info.info = std::move(info);


        } while (0);

        return ret_code;
    }

private:
    std::string service_url_;
};



// call forward

TzMonitorHttpClientHelper::TzMonitorHttpClientHelper(const std::string& service_url) {
    impl_ptr_.reset(new Impl(service_url));
     if (!impl_ptr_) {
         log_crit("create impl failed, CRITICAL!!!!");
         ::abort();
     }
}

TzMonitorHttpClientHelper::~TzMonitorHttpClientHelper(){
}


int TzMonitorHttpClientHelper::http_event_submit(const event_report_t& report) {
    return impl_ptr_->http_event_submit(report);
}

int TzMonitorHttpClientHelper::http_event_query(const event_cond_t& cond, event_query_t& resp) {
    return impl_ptr_->http_event_query(cond, resp);
}

