// 此库的作用就是对业务服务产生的数据进行压缩，然后开辟
// 独立的线程向服务端进行提交，以避免多次独立提交带来的额外消耗

// 如果支持新标准，future更加合适做这个事情

// 客户端使用，尽量减少依赖的库
#include <cassert>
#include <sstream>
#include <thread>
#include <functional>

#ifndef _DEFINE_GET_POINTER_MARKER_
#define _DEFINE_GET_POINTER_MARKER_
template<class T>
T * get_pointer(std::shared_ptr<T> const& p) {
    return p.get();
}
#endif // _DEFINE_GET_POINTER_MARKER_

#include <utils/Log.h>
#include <utils/EQueue.h>
#include <utils/TinyTask.h>

#include "TzMonitorHttpClientHelper.h"
#include "TzMonitorThriftClientHelper.h"
#include "include/TzMonitor.h"

namespace TzMonitor {

typedef std::shared_ptr<std::thread> thread_ptr;

class TzMonitorClient::Impl : public std::enable_shared_from_this<Impl> {
public:
    Impl(std::string host, std::string serv, std::string entity_idx = "1") :
        host_(host), serv_(serv), entity_idx_(entity_idx),
        msgid_(0), current_time_(0) {
        }

    ~Impl() {}

    bool init(const std::string& cfgFile);
    int report_event(const std::string& name, int64_t value, std::string flag = "T");
    int retrieve_stat(const event_cond_t& cond, event_query_t& stat);

    int update_run_cfg(const libconfig::Config& cfg) {

        int ret_code = 0;

        bool cli_enabled;
        if (!cfg.lookupValue("core.cli_enabled", cli_enabled)) {
            log_err("find core.enabled failed");
            ret_code -=1;
        } else {
            if (cli_enabled != cli_enabled_) {
                log_alert("===> update cli_enabled: from %s to %s",
                      cli_enabled_ ? "true": "false", cli_enabled ? "true": "false" );
                cli_enabled_ = cli_enabled;
            }
        }

        int cli_submit_queue_size;
        if (!cfg.lookupValue("core.cli_submit_queue_size", cli_submit_queue_size) || cli_submit_queue_size < 0) {
            log_err("find core.cli_submit_queue_size failed");
            ret_code -=1;
        } else {
            if (cli_submit_queue_size != cli_submit_queue_size_) {
                log_alert("===> update cli_submit_queue_size: from %d to %d",
                          cli_submit_queue_size_, cli_submit_queue_size);
                cli_submit_queue_size_ = cli_submit_queue_size;
            }
        }

        return ret_code;
    }

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

private:
    std::string host_;
    std::string serv_;
    std::string entity_idx_;

    bool cli_enabled_;
    int cli_submit_queue_size_;

    int cli_item_size_per_submit_;
    int cli_il_submit_queue_size_;
    int cli_ob_submit_task_size_;

    // 默认开启一个，当发现待提交队列过长的时候，自动开辟future任务
    thread_ptr thread_run_;
    void run();

    std::shared_ptr<TzMonitorHttpClientHelper> http_agent_;
    std::shared_ptr<TzMonitorThriftClientHelper> thrift_agent_;

    std::mutex lock_;
    int64_t msgid_;
    time_t current_time_;
    std::vector<event_data_t> current_slot_;

    // 自带锁保护
    EQueue<event_report_ptr_t> submit_queue_;
    std::shared_ptr<TinyTask> task_helper_;
    void run_once_task(std::vector<event_report_ptr_t> reports);
};


// Impl member function

bool TzMonitorClient::Impl::init(const std::string& cfgFile) {

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

    if (!cfg.lookupValue("core.cli_enabled", cli_enabled_)) {
        log_info("find core.enabled failed, set default to true");
        cli_enabled_ = true;
    }

    if (!cfg.lookupValue("core.cli_submit_queue_size", cli_submit_queue_size_) ||
        cli_submit_queue_size_ < 0 ) {
        log_info("find core.cli_submit_queue_size failed, set default to 0");
        cli_item_size_per_submit_ = 0;
    }

    if (!cfg.lookupValue("core.cli_item_size_per_submit", cli_item_size_per_submit_) ||
        cli_item_size_per_submit_ <= 0 ) {
        log_info("find core.cli_item_size_per_submit failed, set default to 500");
        cli_item_size_per_submit_ = 500;
    }

    if (!cfg.lookupValue("core.cli_il_submit_queue_size", cli_il_submit_queue_size_) ||
        cli_il_submit_queue_size_ <= 0 ) {
        log_info("find core.cli_il_submit_queue_size failed, set default to 5");
        cli_il_submit_queue_size_ = 5;
    }

    thread_run_.reset(new std::thread(std::bind(&TzMonitorClient::Impl::run, shared_from_this())));
    if (!thread_run_){
        log_err("create run work thread failed! ");
        return false;
    }

    if (!cfg.lookupValue("core.cli_ob_submit_task_size", cli_ob_submit_task_size_) ||
        cli_ob_submit_task_size_ <= 0 ) {
        log_info("find core.cli_ob_submit_task_size failed, set default to 5");
        cli_ob_submit_task_size_ = 5;
    }
    task_helper_ = std::make_shared<TinyTask>(cli_ob_submit_task_size_);
    if (!task_helper_ || !task_helper_->init()){
        log_err("create task_helper work thread failed! ");
        return false;
    }

    log_info("TzMonitorClient init ok!");
    return true;
}

int TzMonitorClient::Impl::report_event(const std::string& name, int64_t value, std::string flag) {

    if (!cli_enabled_) {
        return 0;
    }

    std::lock_guard<std::mutex> lock(lock_);

    event_data_t item {};
    item.name = name;
    item.value = value;
    item.flag = flag;
    item.msgid = ++ msgid_;

    time_t now = ::time(NULL);

    // 因为每一个report都会触发这里的检查，所以不可能过量
    if (current_slot_.size() >= cli_item_size_per_submit_|| now != current_time_ ) {

        assert(current_slot_.size() <= cli_item_size_per_submit_);

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

    if (cli_submit_queue_size_ != 0) {
        submit_queue_.SHRINK_FRONT(cli_submit_queue_size_);
    }

    while (submit_queue_.SIZE() > 2 * cli_item_size_per_submit_) {
        std::vector<event_report_ptr_t> reports;
        size_t ret = submit_queue_.POP(reports, cli_item_size_per_submit_, 5000);
        if (!ret) {
            break;
        }

        std::function<void()> func = std::bind(&TzMonitorClient::Impl::run_once_task, shared_from_this(), reports);
        task_helper_->add_task(func);
    }
    return 0;
}

int TzMonitorClient::Impl::retrieve_stat(const event_cond_t& cond, event_query_t& stat) {

    int ret_code = 0;

    do {

        if (thrift_agent_) {
            ret_code = thrift_agent_->thrift_event_query(cond, stat);
            if (ret_code == 0) {
                break;
            } else {
                log_info("Thrift query return code: %d", ret_code);
                // false through http
            }
        }

        if (http_agent_) {
            ret_code = http_agent_->http_event_query(cond, stat);
            if (ret_code == 0) {
                break;
            } else {
                log_info("Http query return code: %d", ret_code);
            }
        }

        // Error:
        log_err("BAD, reprot failed!");
    } while (0);

    return ret_code;
}

void TzMonitorClient::Impl::run() {

    log_debug("TzMonitorClient submit thread %#lx begin to run ...", (long)pthread_self());
    while (true) {

        event_report_ptr_t report_ptr;
        if( !submit_queue_.POP(report_ptr, 5000) ){
            continue;
        }

        do_report(report_ptr);
    }
}

void TzMonitorClient::Impl::run_once_task(std::vector<event_report_ptr_t> reports) {

    log_debug("TzMonitorClient run_once_task thread %#lx begin to run ...", (long)pthread_self());
    for(auto iter = reports.begin(); iter != reports.end(); ++iter) {
        do_report(*iter);
    }
}



// call forward

TzMonitorClient::TzMonitorClient(std::string host, std::string serv, std::string entity_idx){
    impl_ptr_.reset(new Impl(host, serv, entity_idx));
    if (!impl_ptr_) {
         log_crit("create impl failed, CRITICAL!!!!");
         ::abort();
     }
}

TzMonitorClient::~TzMonitorClient(){}


bool TzMonitorClient::init(const std::string& cfgFile) {
    return impl_ptr_->init(cfgFile);
}

int TzMonitorClient::update_run_cfg(const libconfig::Config& cfg) {
    return impl_ptr_->update_run_cfg(cfg);
}

int TzMonitorClient::report_event(const std::string& name, int64_t value, std::string flag ) {
    return impl_ptr_->report_event(name, value, flag);
}

int TzMonitorClient::retrieve_stat(const event_cond_t& cond, event_query_t& stat) {
    return impl_ptr_->retrieve_stat(cond, stat);
}


int TzMonitorClient::retrieve_stat(const std::string& name, int64_t& count, int64_t& avg, time_t intervel_sec) {

    event_cond_t cond {};
    cond.version =  "1.0.0";
    cond.name = name;
    cond.interval_sec = intervel_sec;
    cond.groupby = GroupType::kGroupNone;

    event_query_t stat {};
    if (impl_ptr_->retrieve_stat(cond, stat) != 0) {
        return -1;
    }

    count = stat.summary.count;
    avg = stat.summary.value_avg;

    return 0;
}

int TzMonitorClient::retrieve_stat(const std::string& name, const std::string& flag, int64_t& count, int64_t& avg, time_t intervel_sec) {

    event_cond_t cond {};
    cond.version =  "1.0.0";
    cond.name = name;
    cond.flag = flag;
    cond.interval_sec = intervel_sec;
    cond.groupby = GroupType::kGroupNone;

    event_query_t stat {};
    if (impl_ptr_->retrieve_stat(cond, stat) != 0) {
        return -1;
    }

    count = stat.summary.count;
    avg = stat.summary.value_avg;

    return 0;
}

int TzMonitorClient::retrieve_stat_flag(const std::string& name, event_query_t& stat, time_t intervel_sec) {

    event_cond_t cond {};
    cond.version =  "1.0.0";
    cond.name = name;
    cond.interval_sec = intervel_sec;
    cond.groupby = GroupType::kGroupbyFlag;

    return impl_ptr_->retrieve_stat(cond, stat);
}

int TzMonitorClient::retrieve_stat_time(const std::string& name, event_query_t& stat, time_t intervel_sec) {

    event_cond_t cond {};
    cond.version =  "1.0.0";
    cond.name = name;
    cond.interval_sec = intervel_sec;
    cond.groupby = GroupType::kGroupbyTime;

    return impl_ptr_->retrieve_stat(cond, stat);
}


int TzMonitorClient::retrieve_stat_time(const std::string& name, const std::string& flag, event_query_t& stat, time_t intervel_sec) {

    event_cond_t cond {};
    cond.version =  "1.0.0";
    cond.name = name;
    cond.flag = flag;
    cond.interval_sec = intervel_sec;
    cond.groupby = GroupType::kGroupbyTime;

    return impl_ptr_->retrieve_stat(cond, stat);
}



} // end namespace

