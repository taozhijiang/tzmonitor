#ifndef _TZ_MONITOR_CLIENT_H__
#define _TZ_MONITOR_CLIENT_H__

// 此库的作用就是对业务服务产生的数据进行压缩，然后开辟
// 独立的线程向服务端进行提交，以避免多次独立提交带来的额外消耗

// 如果支持新标准，future更加合适做这个事情

// 客户端使用，尽量减少依赖的库
#include <cassert>
#include <sstream>
#include <memory>
#include <thread>
#include <functional>


#include <boost/noncopyable.hpp>

#include <libconfig.h++>

#include "Log.h"
#include "EQueue.h"
#include "TzMonitorHttpClientHelper.h"
#include "TzMonitorThriftClientHelper.h"
#include "TinyTask.h"

#ifndef _DEFINE_GET_POINTER_MARKER_
#define _DEFINE_GET_POINTER_MARKER_
template<class T>
T * get_pointer(std::shared_ptr<T> const& p) {
    return p.get();
}
#endif // _DEFINE_GET_POINTER_MARKER_

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
            log_err("I/O error while reading file.");
            return false;
        } catch(libconfig::ParseException &pex) {
            log_err("Parse error at %d - %s", pex.getLine(), pex.getError());
            return false;
        }

        std::string thrift_serv_addr;
        int thrift_listen_port = 0;
        if (!cfg.lookupValue("thrift.serv_addr", thrift_serv_addr) ||
            !cfg.lookupValue("thrift.listen_port", thrift_listen_port) ) {
            log_err("get thrift config failed.");
        } else {
            thrift_agent_ = std::make_shared<TzMonitorThriftClientHelper>(thrift_serv_addr, thrift_listen_port);
        }

        std::string http_serv_addr;
        int http_listen_port = 0;
        if (!cfg.lookupValue("http.serv_addr", http_serv_addr) ||
            !cfg.lookupValue("http.listen_port", http_listen_port) ) {
            log_err("get http config failed.");
        } else {
            std::stringstream ss;
            ss << "http://" << http_serv_addr << ":" << http_listen_port << "/";
            http_agent_ = std::make_shared<TzMonitorHttpClientHelper>(ss.str());
        }

        if (!thrift_agent_ && !http_agent_) {
            log_err("not available agent found!");
            return false;
        }

        if (!cfg.lookupValue("core.max_submit_item_size", max_submit_item_size_) ||
            max_submit_item_size_ <= 0 ) {
            log_info("find core.max_submit_item_size failed, set default to 500");
            max_submit_item_size_ = 500;
        }

        if (!cfg.lookupValue("core.max_submit_queue_size", max_submit_queue_size_) ||
            max_submit_queue_size_ <= 0 ) {
            log_info("find core.max_submit_queue_size failed, set default to 5");
            max_submit_queue_size_ = 5;
        }

        thread_run_.reset(new std::thread(std::bind(&TzMonitorClient::run, shared_from_this())));
        if (!thread_run_){
            log_err("create run work thread failed! ");
            return false;
        }

        if (!cfg.lookupValue("core.max_submit_task_size", max_submit_task_size_) ||
            max_submit_task_size_ <= 0 ) {
            log_info("find core.max_submit_task_size failed, set default to 5");
            max_submit_task_size_ = 5;
        }
        task_helper_ = std::make_shared<TinyTask>(max_submit_task_size_);
        if (!task_helper_ || !task_helper_->init()){
            log_err("create task_helper work thread failed! ");
            return false;
        }

        log_info("TzMonitorClient init ok!");
        return true;
    }

    int report_event(const std::string& name, int64_t value, std::string flag = "T") {

        std::lock_guard<std::mutex> lock(lock_);

        event_data_t item {};
        item.name = name;
        item.value = value;
        item.flag = flag;
        item.msgid = ++ msgid_;

        time_t now = ::time(NULL);

        // 因为每一个report都会触发这里的检查，所以不可能过量
        if (current_slot_.size() >= max_submit_item_size_ || now != current_time_ ) {

            assert(current_slot_.size() <= max_submit_item_size_);

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
        }

        // reset these things
        if (now != current_time_) {
            current_time_ = now;
            msgid_ = 0;
            item.msgid = ++ msgid_; // 新时间，新起点
        }
        current_slot_.emplace_back(item);


        while (submit_queue_.SIZE() > 2 * max_submit_item_size_) {
            std::vector<event_report_ptr_t> reports;
            size_t ret = submit_queue_.POP(reports, max_submit_item_size_, 5000);
            if (!ret) {
                break;
            }

            std::function<void()> func = std::bind(&TzMonitorClient::run_once_task, shared_from_this(), reports);
            task_helper_->add_task(func);
        }
        return 0;
    }

    ~TzMonitorClient() {}

private:
    int do_report(event_report_ptr_t report_ptr) {

        int ret_code = 0;
        if (!report_ptr) {
            return -1;
        }

        do {

            if (thrift_agent_) {
                ret_code = thrift_agent_->thrift_event_submit(*report_ptr);
                if (ret_code == 0) {
                    break;
                } else {
                    log_info("Thrift report submit return code: %d", ret_code);
                    // false through http
                }
            }

            if (http_agent_) {
                ret_code = http_agent_->http_event_submit(*report_ptr);
                if (ret_code == 0) {
                    break;
                } else {
                    log_info("Http report submit return code: %d", ret_code);
                }
            }

            // Error:
            log_err("BAD, reprot failed!");
            ret_code = -2;

        } while (0);

        return ret_code;
    }

    void run() {

        log_debug("TzMonitorClient submit thread %#lx begin to run ...", (long)pthread_self());

        while (true) {

            event_report_ptr_t report_ptr;
            if( !submit_queue_.POP(report_ptr, 5000) ){
                continue;
            }

            do_report(report_ptr);
        }
    }

    void run_once_task(std::vector<event_report_ptr_t> reports) {

        log_debug("TzMonitorClient run_once_task thread %#lx begin to run ...", (long)pthread_self());

        for(auto iter = reports.begin(); iter != reports.end(); ++iter) {
            do_report(*iter);
        }
    }

private:
    std::string host_;
    std::string serv_;
    std::string entity_idx_;

    int max_submit_item_size_;
    int max_submit_queue_size_;
    int max_submit_task_size_;

    // 默认开启一个，当发现待提交队列过长的时候，自动开辟future任务
    thread_ptr thread_run_;

    std::shared_ptr<TzMonitorHttpClientHelper> http_agent_;
    std::shared_ptr<TzMonitorThriftClientHelper> thrift_agent_;

    std::mutex lock_;
    int64_t msgid_;
    time_t current_time_;
    std::vector<event_data_t> current_slot_;

    // 自带锁保护
    EQueue<event_report_ptr_t> submit_queue_;

    std::shared_ptr<TinyTask> task_helper_;
};

} // end namespace

#endif // _TZ_MONITOR_CLIENT_H__
