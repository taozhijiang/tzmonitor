#ifndef _TZ_MONITOR_CLIENT_H__
#define _TZ_MONITOR_CLIENT_H__

// 此库的作用就是对业务服务产生的数据进行压缩，然后开辟
// 独立的线程向服务端进行提交，以避免多次独立提交带来的额外消耗

// 客户端使用，尽量减少依赖的库
#include <cassert>
#include <sstream>
#include <memory>
#include <thread>
#include <functional>
#include <boost/noncopyable.hpp>

#include <libconfig.h++>

#include "EQueue.h"
#include "TzMonitorHttpClientHelper.h"
#include "TzMonitorThriftClientHelper.h"

#ifndef _DEFINE_GET_POINTER_MARKER_
#define _DEFINE_GET_POINTER_MARKER_
template<class T>
T * get_pointer(std::shared_ptr<T> const& p) {
    return p.get();
}
#endif // _DEFINE_GET_POINTER_MARKER_

#define LOG printf

namespace TzMonitor {

typedef std::shared_ptr<std::thread> thread_ptr;

class TzMonitorClient: public boost::noncopyable,
                       public std::enable_shared_from_this<TzMonitorClient> {
public:
    TzMonitorClient(std::string host, std::string serv, std::string entity_idx = "1") :
        host_(host), serv_(serv), entity_idx_(entity_idx),
        msgid_(0), current_time_(0) {
        }

    bool init(const std::string& cfgFile) {

        libconfig::Config cfg;
        try {
            cfg.readFile(cfgFile.c_str());
        } catch(libconfig::FileIOException &fioex) {
            LOG("I/O error while reading file.");
            return false;
        } catch(libconfig::ParseException &pex) {
            LOG("Parse error at %d - %s", pex.getLine(), pex.getError());
            return false;
        }

        std::string thrift_serv_addr;
        int thrift_listen_port = 0;
        if (!cfg.lookupValue("thrift.serv_addr", thrift_serv_addr) ||
            !cfg.lookupValue("thrift.listen_port", thrift_listen_port) ){
            LOG("get thrift config failed.");
        } else {
            thrift_agent_ = std::make_shared<TzMonitorThriftClientHelper>(thrift_serv_addr, thrift_listen_port);
        }

        std::string http_serv_addr;
        int http_listen_port = 0;
        if (!cfg.lookupValue("http.serv_addr", http_serv_addr) ||
            !cfg.lookupValue("http.listen_port", http_listen_port) ){
            LOG("get http config failed.");
        } else {
            std::stringstream ss;
            ss << "http://" << http_serv_addr << ":" << http_listen_port << "/";
            http_agent_ = std::make_shared<TzMonitorHttpClientHelper>(ss.str());
        }

        if (!thrift_agent_ && !http_agent_) {
            LOG("not available agent found!");
            return false;
        }

        thread_ptr t_thread(new std::thread(std::bind(&TzMonitorClient::run, shared_from_this())));
        if (!t_thread){
            LOG("create work thread failed! ");
            return false;
        }

        threads_.push_back(t_thread);
        return true;
    }

    void run() {

        LOG("TzMonitorClient submit thread %#lx begin to run ...", (long)pthread_self());

        while (true) {

            event_report_ptr_t report_ptr;
            if( !submit_queue_.POP(report_ptr, 5000) ){
                continue;
            }

            if (!report_ptr) {
                LOG("POP event_report_t is empty!");
                continue;
            }

            int  ret_code = -1;
            if (thrift_agent_) {
                ret_code = thrift_agent_->thrift_event_submit(*report_ptr);
                if (ret_code == 0) {
                    continue;
                } else {
                    LOG("Thrift report submit return code: %d", ret_code);
                    // false through http
                }
            }

            if (http_agent_) {
                ret_code = http_agent_->http_event_submit(*report_ptr);
                if (ret_code == 0) {
                    continue;
                } else {
                    LOG("Http report submit return code: %d", ret_code);
                }
            }

            // Error:
            LOG("BAD, reprot failed!");
            assert(false);
        }

    }

    int report_event(const std::string& name, int64_t value, std::string flag = "T") {

        std::lock_guard<std::mutex> lock(lock_);

        event_data_t item {};
        item.name = name;
        item.value = value;
        item.flag = flag;
        item.msgid = ++ msgid_;

        time_t now = ::time(NULL);

        if (now != current_time_) {
            if (!current_slot_.empty()) {

                event_report_ptr_t report_ptr = std::make_shared<event_report_t>();
                report_ptr->version = "1.0.0";
                report_ptr->time = current_time_;
                report_ptr->host = host_;
                report_ptr->serv = serv_;
                report_ptr->entity_idx = entity_idx_;
                std::swap(current_slot_, report_ptr->data);

                submit_queue_.PUSH(report_ptr);
            }

            // reset these things
            current_time_ = now;
            msgid_ = 0;

        }

        current_slot_.emplace_back(item);
        return 0;
    }

    ~TzMonitorClient() {}

private:
    std::string host_;
    std::string serv_;
    std::string entity_idx_;

    // 默认开启一个，当发现待提交队列过长的时候，自动开辟独立的工作线程
    std::vector<thread_ptr> threads_;

    std::shared_ptr<TzMonitorHttpClientHelper> http_agent_;
    std::shared_ptr<TzMonitorThriftClientHelper> thrift_agent_;

    std::mutex lock_;
    int64_t msgid_;
    time_t current_time_;
    std::vector<event_data_t> current_slot_;

    // 自带锁保护
    EQueue<event_report_ptr_t> submit_queue_;

};

} // end namespace

#endif // _TZ_MONITOR_CLIENT_H__
