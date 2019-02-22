/*-
 * Copyright (c) 2018 TAO Zhijiang<taozhijiang@gmail.com>
 *
 * Licensed under the BSD-3-Clause license, see LICENSE for full information.
 *
 */


// 此库的作用就是对业务服务产生的数据进行压缩，然后开辟
// 独立的线程向服务端进行提交，以避免多次独立提交带来的额外消耗

// 如果支持新标准，future更加合适做这个事情

// 客户端使用，尽量减少依赖的库

#include <xtra_rhel6.h>

#include <unistd.h>

#include <cassert>
#include <sstream>
#include <thread>
#include <functional>

#include <Utils/EQueue.h>
#include <Utils/TinyTask.h>

#include <Client/RpcClient.h>
#include <Client/include/MonitorClient.h>

#include <Client/MonitorRpcClientHelper.h>

#include <Client/LogClient.h>

namespace tzmonitor_client {


struct MonitorClientConf {

    boost::atomic<bool> report_enabled_;

    boost::atomic<int> report_queue_limit_;
    boost::atomic<int> size_per_report_;

    boost::atomic<int> additional_report_queue_size_;
    boost::atomic<int> support_report_task_size_;

    MonitorClientConf():
        report_enabled_(true),
        report_queue_limit_(0),
        size_per_report_(200),
        additional_report_queue_size_(5),
        support_report_task_size_(2) {
    }
};

class MonitorClient::Impl : public std::enable_shared_from_this<Impl> {
public:
    Impl(std::string service, std::string entity_idx) :
        service_(service), entity_idx_(entity_idx),
        client_agent_(),
        thread_run_(),
        task_helper_(),
        lock_(),
        msgid_(0), current_time_(0),
        current_slot_(),
        submit_queue_(),
        conf_() {
    }

    ~Impl() {}

    bool init(const std::string& cfgFile, CP_log_store_func_t log_func);
    bool init(const libconfig::Setting& setting, CP_log_store_func_t log_func);

    int report_event(const std::string& metric, int64_t value, const std::string& tag);
    int select_stat(const event_cond_t& cond, event_select_t& stat);

private:

    void report_empty_event() {
        report_event("", 0, "T");
    }

    int do_report(event_report_ptr_t report_ptr) {

        if (!report_ptr) {
            return -1;
        }

        if (!client_agent_) {
            log_err("MonitorRpcClientHelper not initialized, fatal!");
            return -1;
        }

        auto code = client_agent_->rpc_event_submit(*report_ptr);
        if (code == 0) {
            log_debug("report submit ok.");
            return 0;
        }

        log_err("report submit return code: %d", code);
        return code;
    }

private:
    const std::string service_;
    const std::string entity_idx_;

    std::shared_ptr<MonitorRpcClientHelper> client_agent_;

    // 默认开启一个提交，当发现待提交队列过长的时候，自动开辟future任务
    std::shared_ptr<boost::thread> thread_run_;
    void run();

    std::shared_ptr<tzrpc::TinyTask> task_helper_;
    void run_once_task(std::vector<event_report_ptr_t> reports);


    // used int report procedure
    std::mutex lock_;

    int64_t msgid_;
    time_t  current_time_;
    std::vector<event_data_t> current_slot_;

    tzrpc::EQueue<event_report_ptr_t> submit_queue_;

    MonitorClientConf conf_;
};


// Impl member function


bool MonitorClient::Impl::init(const std::string& cfgFile, CP_log_store_func_t log_func) {

    libconfig::Config cfg;
    try {
        cfg.readFile(cfgFile.c_str());

        const libconfig::Setting& setting = cfg.lookup("rpc_monitor_client");
        return init(setting, log_func);

    } catch(libconfig::FileIOException &fioex) {
        log_err("I/O error while reading file.");
        return false;
    } catch(libconfig::ParseException &pex) {
        log_err("Parse error at %d - %s", pex.getLine(), pex.getError());
        return false;
    } catch (...) {
        log_err("process cfg failed.");
    }

    return false;
}

bool MonitorClient::Impl::init(const libconfig::Setting& setting, CP_log_store_func_t log_func) {

    // init log first
    set_checkpoint_log_store_func(log_func);

    std::string serv_addr;
    int listen_port = 0;
    if (!setting.lookupValue("serv_addr", serv_addr) ||
        !setting.lookupValue("listen_port", listen_port) ||
        serv_addr.empty() || listen_port <= 0) {
        log_err("get rpc server addr config failed.");
    }

    client_agent_ = std::make_shared<MonitorRpcClientHelper>(serv_addr, listen_port);
    if (!client_agent_) {
        log_err("not available agent found!");
        return false;
    }


    // ping test

    // load conf


    thread_run_.reset(new boost::thread(std::bind(&MonitorClient::Impl::run, shared_from_this())));
    if (!thread_run_){
        log_err("create run work thread failed! ");
        return false;
    }

    task_helper_ = std::make_shared<tzrpc::TinyTask>(conf_.support_report_task_size_);
    if (!task_helper_ || !task_helper_->init()){
        log_err("create task_helper work thread failed! ");
        return false;
    }

    log_info("MonitorClient init ok!");
    return true;
}

int MonitorClient::Impl::report_event(const std::string& metric, int64_t value, const std::string& tag) {

    if (!conf_.report_enabled_) {
        return 0;
    }

    std::lock_guard<std::mutex> lock(lock_);

    event_data_t item {};
    item.metric = metric;
    item.value = value;
    item.tag = tag;
    item.msgid = ++ msgid_;

    time_t now = ::time(NULL);

    // 因为每一个report都会触发这里的检查，所以不可能过量
    if (current_slot_.size() >= conf_.size_per_report_ || now != current_time_) {

        assert(current_slot_.size() <= conf_.size_per_report_);

        if (!current_slot_.empty()) {
            event_report_ptr_t report_ptr = std::make_shared<event_report_t>();
            report_ptr->version = "1.0.0";
            report_ptr->timestamp = current_time_;
            report_ptr->service = service_;
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

    // item.name 可能是空的，我们会定期插入空的消息，强制没有满的消息刷新提交出去
    // 空消息就是metric为0的消息
    if (likely(!item.metric.empty())) {
        current_slot_.emplace_back(item);
    }

    if (conf_.report_queue_limit_ != 0) {
        submit_queue_.SHRINK_FRONT(conf_.report_queue_limit_);
    }

    while (submit_queue_.SIZE() > 2 * conf_.size_per_report_) {
        std::vector<event_report_ptr_t> reports;
        size_t ret = submit_queue_.POP(reports, conf_.size_per_report_, 2000);
        if (!ret) {
            break;
        }

        std::function<void()> func = std::bind(&MonitorClient::Impl::run_once_task, shared_from_this(), reports);
        task_helper_->add_additional_task(func);
    }
    return 0;
}

int MonitorClient::Impl::select_stat(const event_cond_t& cond, event_select_t& stat) {

    if (!client_agent_) {
        log_err("MonitorRpcClientHelper not initialized, fatal!");
        return -1;
    }

    auto code = client_agent_->rpc_event_select(cond, stat);
    if (code == 0) {
        log_debug("event select ok.");
        return 0;
    }

    log_err("event select return code: %d", code);
    return code;
}

void MonitorClient::Impl::run() {

    log_debug("MonitorClient submit thread %#lx begin to run ...", (long)pthread_self());

    while (true) {

        event_report_ptr_t report_ptr;
        time_t start = ::time(NULL);
        if( !submit_queue_.POP(report_ptr, 1000) ){
            report_empty_event(); // 触发事件提交
            continue;
        }

        do_report(report_ptr);
        if (::time(NULL) != start) {
            report_empty_event(); // 触发事件提交
        }
    }
}

void MonitorClient::Impl::run_once_task(std::vector<event_report_ptr_t> reports) {

    log_debug("MonitorClient run_once_task thread %#lx begin to run ...", (long)pthread_self());
    for(auto iter = reports.begin(); iter != reports.end(); ++iter) {
        do_report(*iter);
    }
}



// call forward
MonitorClient::MonitorClient(std::string entity_idx) {

    char host[64 + 1] {};
    ::gethostname(host, 64);

    impl_ptr_.reset(new Impl(program_invocation_short_name, entity_idx));
    if (!impl_ptr_) {
         log_crit("create impl failed, CRITICAL!!!!");
         ::abort();
     }
}

MonitorClient::MonitorClient(std::string service, std::string entity_idx){

    impl_ptr_.reset(new Impl(service, entity_idx));
    if (!impl_ptr_) {
         log_crit("create impl failed, CRITICAL!!!!");
         ::abort();
     }
}

MonitorClient::~MonitorClient(){}


bool MonitorClient::init(const std::string& cfgFile, CP_log_store_func_t log_func) {
    return impl_ptr_->init(cfgFile, log_func);
}

bool MonitorClient::init(const libconfig::Setting& setting, CP_log_store_func_t log_func) {
    return impl_ptr_->init(setting, log_func);
}

int MonitorClient::report_event(const std::string& name, int64_t value, std::string flag ) {
    return impl_ptr_->report_event(name, value, flag);
}

int MonitorClient::select_stat(const event_cond_t& cond, event_select_t& stat) {
    return impl_ptr_->select_stat(cond, stat);
}


int MonitorClient::select_stat(const std::string& metric, int64_t& count, int64_t& avg, time_t tm_intervel) {

    event_cond_t cond {};
    cond.version =  "1.0.0";
    cond.metric  = metric;
    cond.tm_interval = tm_intervel;
    cond.groupby = GroupType::kGroupNone;

    event_select_t stat {};
    if (impl_ptr_->select_stat(cond, stat) != 0) {
        return -1;
    }

    count = stat.summary.count;
    avg = stat.summary.value_avg;

    return 0;
}

int MonitorClient::select_stat(const std::string& metric, const std::string& tag,
                               int64_t& count, int64_t& avg, time_t tm_intervel) {

    event_cond_t cond {};
    cond.version =  "1.0.0";
    cond.metric = metric;
    cond.tag = tag;
    cond.tm_interval = tm_intervel;
    cond.groupby = GroupType::kGroupNone;

    event_select_t stat {};
    if (impl_ptr_->select_stat(cond, stat) != 0) {
        return -1;
    }

    count = stat.summary.count;
    avg = stat.summary.value_avg;

    return 0;
}

int MonitorClient::select_stat_by_tag(const std::string& metric,
                                      event_select_t& stat, time_t tm_intervel) {

    event_cond_t cond {};
    cond.version =  "1.0.0";
    cond.metric = metric;
    cond.tm_interval = tm_intervel;
    cond.groupby = GroupType::kGroupbyTag;

    return impl_ptr_->select_stat(cond, stat);
}

int MonitorClient::select_stat_by_time(const std::string& metric,
                                       event_select_t& stat, time_t tm_intervel) {

    event_cond_t cond {};
    cond.version =  "1.0.0";
    cond.metric = metric;
    cond.tm_interval = tm_intervel;
    cond.groupby = GroupType::kGroupbyTimestamp;

    return impl_ptr_->select_stat(cond, stat);
}


int MonitorClient::select_stat_by_time(const std::string& metric, const std::string& tag,
                                       event_select_t& stat, time_t tm_intervel) {

    event_cond_t cond {};
    cond.version =  "1.0.0";
    cond.metric = metric;
    cond.tag = tag;
    cond.tm_interval = tm_intervel;
    cond.groupby = GroupType::kGroupbyTimestamp;

    return impl_ptr_->select_stat(cond, stat);
}



} // end namespace tzmonitor_client

