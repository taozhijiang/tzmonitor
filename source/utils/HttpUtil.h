#ifndef _TZ_HTTP_UTIL_H_
#define _TZ_HTTP_UTIL_H_

#include <memory>
#include <vector>
#include <list>
#include <string>
#include <cstring>

#include <curl/curl.h>
#include <boost/noncopyable.hpp>

#include "CryptoUtil.h"

namespace HttpUtil {


static int generate_url(const std::string &strUrl, const std::map<std::string, std::string> &fields,
                        std::string& strCallUrl) {

    if (strUrl.empty())
        return -1;

    if (fields.empty()) {
        strCallUrl = strUrl;
        return 0;
    }

    std::ostringstream oss;
    std::ostringstream ossUrl;

    for (auto iter = fields.begin(); iter != fields.end(); ++iter) {

        oss << iter->first << "=";
        oss << iter->second << "&";

        ossUrl << iter->first << "=";
        ossUrl << CryptoUtil::url_encode(iter->second); //url encode
        ossUrl << "&";
    }

    // url_encode之前算签名
    std::string need_sign = oss.str();
    if (need_sign[need_sign.size() - 1] == '&') {
        need_sign.erase(need_sign.end() - 1);
    }

    std::string sha1value = CryptoUtil::sha1(need_sign);
    std::string sha1str = CryptoUtil::hex_string(sha1value.c_str(), sha1value.length());

    ossUrl<<"sign="<<sha1str;
    strCallUrl = strUrl + '?' + ossUrl.str();

    return 0;
}

static bool check_sha1(const std::map<std::string, std::string> &fields, const std::string& sign) {
    if (fields.empty() || sign.empty()) {
        return false;
    }

    std::ostringstream oss;

    for (auto iter = fields.begin(); iter != fields.end(); ++iter) {
        oss << iter->first << "=";
        oss << CryptoUtil::url_decode(iter->second) << "&";
    }

    // url_encode之前算签名
    std::string need_sign = oss.str();
    if (need_sign[need_sign.size() - 1] == '&') {
        need_sign.erase(need_sign.end() - 1);
    }

    std::string sha1value = CryptoUtil::sha1(need_sign);
    std::string sha1str = CryptoUtil::hex_string(sha1value.c_str(), sha1value.length());

    return sha1value == sign;
}

class HttpClient : boost::noncopyable {
public:
    HttpClient(){}
    ~HttpClient(){}

    int GetByHttp(const std::string& strUrl, long nConnTimeout = 120, long nTimeout = 200 ){

        int ret_code = 0;
        response_data_.clear();

        fprintf(stderr,"get url: %s", strUrl.c_str());

        CURL *curl = curl_easy_init();
        curl_easy_setopt(curl, CURLOPT_ENCODING , "UTF-8");
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 1);
        curl_easy_setopt(curl, CURLOPT_POST, 0);
        curl_easy_setopt(curl, CURLOPT_URL, strUrl.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, this);
        curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1);
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, nConnTimeout);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, nTimeout);

        CURLcode res = curl_easy_perform(curl);
        response_data_.push_back(0);

        long nStatusCode = 500;
        if (res == CURLE_OK) {
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &nStatusCode);
        }
        curl_easy_cleanup(curl);
        if (res == CURLE_OK && nStatusCode == 200  ) { // OK
        } else {
            fprintf(stderr, "curl nStatusCode:%ld, res=%d, error:%s", nStatusCode, res, curl_easy_strerror(res));
            ret_code = -1;
        }

        return ret_code;
    }

    int PostByHttp(const std::string& strUrl, const std::string& strData,
                   long nConnTimeout = 120, long nTimeout = 200 ) {

        int ret_code = 0;
        response_data_.clear();

        fprintf(stderr,"post url: %s, data: %s", strUrl.c_str(), strData.c_str());

        CURL *curl = curl_easy_init();
        curl_easy_setopt(curl, CURLOPT_ENCODING , "UTF-8");
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 1);
        curl_easy_setopt(curl, CURLOPT_POST, 1);
        curl_easy_setopt(curl, CURLOPT_URL, strUrl.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, strData.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, this);
        curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1);
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, nConnTimeout);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, nTimeout);

        CURLcode res = curl_easy_perform(curl);
        response_data_.push_back(0);

        long nStatusCode = 500;
        if (res == CURLE_OK) {
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &nStatusCode);
        }
        curl_easy_cleanup(curl);
        if (res == CURLE_OK && nStatusCode == 200){
        } else {
            fprintf(stderr, "nStatusCode:%ld res=%d,error:%s", nStatusCode, res, curl_easy_strerror(res));
            ret_code = -1;
        }

        return ret_code;
    }


    int PostByHttp(const std::string& strUrl, const std::string& strData, const std::list<std::string> &headers,
                   long nConnTimeout = 120, long nTimeout = 200 ) {

        int ret_code = 0;
        response_data_.push_back(0);

        fprintf(stderr,"post url: %s, data: %s", strUrl.c_str(), strData.c_str());

        CURL *curl = curl_easy_init();
        curl_easy_setopt(curl, CURLOPT_ENCODING , "UTF-8");
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 1);
        curl_easy_setopt(curl, CURLOPT_POST, 1);
        curl_easy_setopt(curl, CURLOPT_URL, strUrl.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, strData.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, this);
        curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1);
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, nConnTimeout);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, nTimeout);

        struct curl_slist *s_headers=NULL;
        std::list<std::string>::const_iterator it;
        for (it=headers.begin(); it!=headers.end(); it++) {
            s_headers = curl_slist_append(s_headers, it->c_str());
        }
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, s_headers);

        CURLcode res = curl_easy_perform(curl);
        curl_slist_free_all(s_headers);
        response_data_.push_back(0);

        long nStatusCode = 500;
        if (res == CURLE_OK) {
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &nStatusCode);
        }
        curl_easy_cleanup(curl);
        if (res == CURLE_OK && nStatusCode == 200) {
        } else {
            fprintf(stderr, "nStatusCode:%ld res=%d,error:%s", nStatusCode, res, curl_easy_strerror(res));
            ret_code = -1;
        }

        return ret_code;
    }


    char* GetData() {
        return response_data_.data();
    }

private:

    // static member func, 所以需要使用指针调用其非静态成员
    static size_t WriteCallback(void* ptr, size_t size, size_t nmemb, void* data) {

        HttpClient* pHttp = static_cast<HttpClient*>(data);
        size_t pos = pHttp->response_data_.size();
        size_t len = size  * nmemb;
        pHttp->response_data_.resize(pos + len);
        char* pRespBuff = pHttp->response_data_.data();
        ::memcpy(pRespBuff + pos, ptr, len);

        return len;
    }

private:
    std::vector<char> response_data_;
};

} // end namespace


#endif // _TZ_HTTP_UTIL_H_
