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

    boost::atomic<int> additional_report_step_size_;
    boost::atomic<int> support_report_task_size_;

    MonitorClientConf():
        report_enabled_(true),
        report_queue_limit_(0),
        size_per_report_(5000),
        additional_report_step_size_(10),
        support_report_task_size_(5) {
    }
};

class MonitorClientImpl: public boost::noncopyable {

    friend class MonitorClient;

private:

    bool init(const std::string& cfgFile, CP_log_store_func_t log_func);
    bool init(const libconfig::Setting& setting, CP_log_store_func_t log_func);
    bool init(const std::string& service, const std::string& entity_idx,
              const std::string& addr, uint16_t port, CP_log_store_func_t log_func);

    int ping();
    int report_event(const std::string& metric, int64_t value, const std::string& tag);
    int select_stat(event_cond_t& cond, event_select_t& stat);

    int known_metrics(const std::string& version, const std::string& service,
                      event_handler_conf_t& handler_conf, std::vector<std::string>& metrics);
    int known_services(const std::string& version, std::vector<std::string>& services);

    int update_runtime_conf(const libconfig::Config& conf);
    int module_status(std::string& strModule, std::string& strKey, std::string& strValue);

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

    // 新的线程创建新的提交连接，否则并没有增加并发量
    int do_additional_report(std::shared_ptr<MonitorRpcClientHelper>& client_agent, event_report_ptr_t report_ptr) {

        if (!report_ptr || !client_agent) {
            return -1;
        }

        auto code = client_agent->rpc_event_submit(*report_ptr);
        if (code == 0) {
            log_debug("report submit ok.");
            return 0;
        }

        log_err("report submit return code: %d", code);
        return code;
    }

private:
    // 确保 service和entity_idx已经是定义良好的了
    MonitorClientImpl(std::string service, std::string entity_idx) :
        service_(service), entity_idx_(entity_idx),
        client_agent_(),
        thread_run_(),
        task_helper_(),
        lock_(),
        msgid_(0), current_time_(0),
        current_slot_(),
        submit_queue_(),
        already_initialized_(false),
        monitor_addr_(),
        monitor_port_(),
        conf_() {
    }

    ~MonitorClientImpl() {}

    static MonitorClientImpl& instance();

private:

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

    // 该单例只允许初始化一次
    bool already_initialized_;
    std::string service_;
    std::string entity_idx_;
    std::string monitor_addr_;
    uint16_t    monitor_port_;

    MonitorClientConf conf_;
};


// Impl member function

MonitorClientImpl& MonitorClientImpl::instance() {
    static MonitorClientImpl helper(program_invocation_short_name, "");
    return helper;
}

bool MonitorClientImpl::init(const std::string& cfgFile, CP_log_store_func_t log_func) {

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

bool MonitorClientImpl::init(const libconfig::Setting& setting, CP_log_store_func_t log_func) {

    // init log first
    set_checkpoint_log_store_func(log_func);
    log_init(7);

    std::string serv_addr;
    int listen_port = 0;
    if (!setting.lookupValue("serv_addr", serv_addr) ||
        !setting.lookupValue("listen_port", listen_port) ||
        serv_addr.empty() || listen_port <= 0) {
        printf("get rpc server addr config failed.");
        return false;
    }

    // conf update
    bool value_b;
    int  value_i;

    if (setting.lookupValue("report_enabled", value_b)) {
        log_notice("update report_enabled from %s to %s",
                   (conf_.report_enabled_ ? "on" : "off"), (value_b ? "on" : "off") );
        conf_.report_enabled_ = value_b;
    }

    if (setting.lookupValue("report_queue_limit", value_i) && value_i >= 0) {
        log_notice("update report_queue_limit from %d to %d",
                   conf_.report_queue_limit_.load(), value_i );
        conf_.report_queue_limit_ = value_i;
    }

    if (setting.lookupValue("size_per_report", value_i) && value_i > 0) {
        log_notice("update size_per_report from %d to %d",
                   conf_.size_per_report_.load(), value_i );
        conf_.size_per_report_ = value_i;
    }

    if (setting.lookupValue("additional_report_step_size", value_i) && value_i >= 0) {
        log_notice("update additional_report_step_size from %d to %d",
                   conf_.additional_report_step_size_.load(), value_i );
        conf_.additional_report_step_size_ = value_i;
    }

    if (setting.lookupValue("support_report_task_size", value_i) && value_i > 0) {
        log_notice("update support_report_task_size from %d to %d",
                   conf_.support_report_task_size_.load(), value_i );
        conf_.support_report_task_size_ = value_i;
    }

    std::string service;
    std::string entity_idx;
    setting.lookupValue("service", service);
    setting.lookupValue("entity_idx", entity_idx);

    // load other conf

    return init(service, entity_idx, serv_addr, listen_port, log_func);
}


bool MonitorClientImpl::init(const std::string& service, const std::string& entity_idx,
                             const std::string& addr, uint16_t port, CP_log_store_func_t log_func) {


    std::lock_guard<std::mutex> lock(lock_);

    if (already_initialized_) {
        log_err("MonitorClientImpl already successfully initialized...");
        return true;
    }

    // init log first
    set_checkpoint_log_store_func(log_func);
    log_init(7);

    if (!service.empty()) {
        log_notice("override service from %s to %s", service_.c_str(), service.c_str());
        service_ = service;
    }

    if (!entity_idx.empty()) {
        log_notice("override entity_idx from %s to %s", entity_idx_.c_str(), entity_idx.c_str());
        entity_idx_ = entity_idx;
    }

    monitor_addr_ = addr;
    monitor_port_ = port;

    if (service_.empty() || monitor_addr_.empty() || monitor_port_ == 0) {
        log_err("critical param error: service %s, addr %s, port %u",
                service_.c_str(), monitor_addr_.c_str(), monitor_port_);
        return false;
    }

    client_agent_ = std::make_shared<MonitorRpcClientHelper>(monitor_addr_, monitor_port_);
    if (!client_agent_) {
        log_err("not available agent found!");
        return false;
    }

    if (client_agent_->rpc_ping() != 0) {
        log_err("client agent ping test failed...");
        return false;
    }

    thread_run_.reset(new boost::thread(std::bind(&MonitorClientImpl::run, this)));
    if (!thread_run_){
        log_err("create run work thread failed! ");
        return false;
    }

    task_helper_ = std::make_shared<tzrpc::TinyTask>(conf_.support_report_task_size_);
    if (!task_helper_ || !task_helper_->init()){
        log_err("create task_helper work thread failed! ");
        return false;
    }

    log_info("MonitorClientImpl init ok!");
    already_initialized_ = true;

    return true;
}

// rpc_monitor_client
int MonitorClientImpl::update_runtime_conf(const libconfig::Config& conf) {

    try {

        // initialize client conf
        const libconfig::Setting& setting = conf.lookup("rpc_monitor_client");

        // conf update
        bool value_b;
        int  value_i;

        if (setting.lookupValue("report_enabled", value_b)) {
            log_notice("update report_enabled from %s to %s",
                       (conf_.report_enabled_ ? "on" : "off"), (value_b ? "on" : "off") );
            conf_.report_enabled_ = value_b;
        }

        if (setting.lookupValue("report_queue_limit", value_i) && value_i >= 0) {
            log_notice("update report_queue_limit from %d to %d",
                       conf_.report_queue_limit_.load(), value_i );
            conf_.report_queue_limit_ = value_i;
        }

        if (setting.lookupValue("size_per_report", value_i) && value_i > 0) {
            log_notice("update size_per_report from %d to %d",
                       conf_.size_per_report_.load(), value_i );
            conf_.size_per_report_ = value_i;
        }

        if (setting.lookupValue("additional_report_step_size", value_i) && value_i >= 0) {
            log_notice("update additional_report_step_size from %d to %d",
                       conf_.additional_report_step_size_.load(), value_i );
            conf_.additional_report_step_size_ = value_i;
        }

        return 0;

    } catch (const libconfig::SettingNotFoundException &nfex) {
        log_err("rpc_monitor_client not found!");
    } catch (std::exception& e) {
        log_err("execptions catched for %s",  e.what());
    }

    return -1;
}

int MonitorClientImpl::module_status(std::string& strModule, std::string& strKey, std::string& strValue) {

    strModule = "tzmonitor_client";
    // service + entity_idx

    strKey = service_;
    if (!entity_idx_.empty()) {
        strKey += "!";
        strKey += entity_idx_;
    }

    std::stringstream ss;

    ss << "\t" << "service: " << service_ << std::endl;
    ss << "\t" << "entity_idx: " << entity_idx_ << std::endl;

    ss << "\t" << "report_enable: " << (conf_.report_enabled_ ? "true" : "false") << std::endl;
    ss << "\t" << "size_per_report: " << conf_.size_per_report_ << std::endl;
    ss << "\t" << "report_queue_limit: " << conf_.report_queue_limit_ << std::endl;
    ss << "\t" << "current_queue: " << submit_queue_.SIZE() << std::endl;

    ss << "\t" << std::endl;

    ss << "\t" << "additional_step_size: " << conf_.additional_report_step_size_ << std::endl;
    ss << "\t" << "support_task_size: " << conf_.support_report_task_size_ << std::endl;

    strValue = ss.str();

    return 0;
}


int MonitorClientImpl::ping() {

    if (!client_agent_) {
        log_err("MonitorRpcClientHelper not initialized, fatal!");
        return -1;
    }

    auto code = client_agent_->rpc_ping();
    if (code == 0) {
        log_debug("ping test ok.");
        return 0;
    }

    log_err("ping return code: %d", code);
    return code;
}


int MonitorClientImpl::report_event(const std::string& metric, int64_t value, const std::string& tag) {

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

        // 32位超过后回转，如果每一秒回转会在linger优化中判为重复消息
        if (msgid_ >= std::numeric_limits<int32_t>::max()) {
            msgid_ = 0;
        }

        item.msgid = ++ msgid_; // 新时间，新起点
    }

    // item.name 可能是空的，我们会定期插入空的消息，强制没有满的消息刷新提交出去
    // 空消息就是metric为0的消息
    if (likely(!item.metric.empty())) {
        current_slot_.emplace_back(item);
    }

    while (submit_queue_.SIZE() > 2 * conf_.additional_report_step_size_) {
        std::vector<event_report_ptr_t> reports;
        size_t ret = submit_queue_.POP(reports, conf_.additional_report_step_size_, 10);
        if (!ret) {
            break;
        }

        log_notice("MonitorClient additional task with item %lu", ret);
        std::function<void()> func = std::bind(&MonitorClientImpl::run_once_task, this, reports);
        task_helper_->add_additional_task(func);
    }

    return 0;
}

int MonitorClientImpl::select_stat(event_cond_t& cond, event_select_t& stat) {

    if (!client_agent_) {
        log_err("MonitorRpcClientHelper not initialized, fatal!");
        return -1;
    }

    // 如果为空才覆盖
    if (cond.service.empty()) {
        cond.service = service_;
    }
    auto code = client_agent_->rpc_event_select(cond, stat);
    if (code == 0) {
        log_debug("event select ok.");
        return 0;
    }

    log_err("event select return code: %d", code);
    return code;
}


int MonitorClientImpl::known_metrics(const std::string& version, const std::string& service,
                                     event_handler_conf_t& handler_conf, std::vector<std::string>& metrics) {

    if (!client_agent_) {
        log_err("MonitorRpcClientHelper not initialized, fatal!");
        return -1;
    }

    std::string service_t = service_;
    if (!service.empty()) {
        service_t = service;
    }
    auto code = client_agent_->rpc_known_metrics(version, service_t, handler_conf, metrics);
    if (code == 0) {
        log_debug("known metrics ok.");
        return 0;
    }

    log_err("known metrics return code: %d", code);
    return code;
}

int MonitorClientImpl::known_services(const std::string& version, std::vector<std::string>& services) {

    if (!client_agent_) {
        log_err("MonitorRpcClientHelper not initialized, fatal!");
        return -1;
    }

    auto code = client_agent_->rpc_known_services(version, services);
    if (code == 0) {
        log_debug("known services ok.");
        return 0;
    }

    log_err("known services return code: %d", code);
    return code;
}



void MonitorClientImpl::run() {

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

        if (conf_.report_queue_limit_ != 0 && submit_queue_.SIZE() > conf_.report_queue_limit_) {
            log_err("about to shrink submit_queue, current %lu, limit %d",
                    submit_queue_.SIZE(), conf_.report_queue_limit_.load());
            submit_queue_.SHRINK_FRONT(conf_.report_queue_limit_);
        }
    }
}

void MonitorClientImpl::run_once_task(std::vector<event_report_ptr_t> reports) {

    log_debug("MonitorClient run_once_task thread %#lx begin to run ...", (long)pthread_self());

    auto client_agent = std::make_shared<MonitorRpcClientHelper>(monitor_addr_, monitor_port_);
    if (!client_agent) {
        log_err("not available agent found!");
        return;
    }

    if (client_agent->rpc_ping() != 0) {
        log_err("client agent ping test failed...");
        return;
    }

    for(auto iter = reports.begin(); iter != reports.end(); ++iter) {
        do_additional_report(client_agent, *iter);
    }

    if (conf_.report_queue_limit_ != 0 && submit_queue_.SIZE() > conf_.report_queue_limit_) {
        log_err("about to shrink submit_queue, current %lu, limit %d",
                submit_queue_.SIZE(), conf_.report_queue_limit_.load());
        submit_queue_.SHRINK_FRONT(conf_.report_queue_limit_);
    }
}



// call forward
MonitorClient::MonitorClient(std::string entity_idx) {
    (void)MonitorClientImpl::instance();
}

MonitorClient::MonitorClient(std::string service, std::string entity_idx){
    (void)MonitorClientImpl::instance();
}

MonitorClient::~MonitorClient(){}


bool MonitorClient::init(const std::string& cfgFile, CP_log_store_func_t log_func) {
    return MonitorClientImpl::instance().init(cfgFile, log_func);
}

bool MonitorClient::init(const libconfig::Setting& setting, CP_log_store_func_t log_func) {
    return MonitorClientImpl::instance().init(setting, log_func);
}

bool MonitorClient::init(const std::string& addr,  uint16_t port, CP_log_store_func_t log_func) {
    return MonitorClientImpl::instance().init("", "", addr, port, log_func);
}

bool MonitorClient::init(const std::string& service, const std::string& entity_idx,
                         const std::string& addr, uint16_t port,
                         CP_log_store_func_t log_func) {
    return MonitorClientImpl::instance().init(service, entity_idx, addr, port, log_func);
}


// 初始化检查提前做，将开销分担到各个调用线程中去。

int MonitorClient::report_event(const std::string& name, int64_t value, std::string flag ) {

    if (unlikely(!MonitorClientImpl::instance().already_initialized_)) {
        log_err("MonitorClientImpl not initialized...");
        return -1;
    }

    return MonitorClientImpl::instance().report_event(name, value, flag);
}

int MonitorClient::ping() {

    if (unlikely(!MonitorClientImpl::instance().already_initialized_)) {
        log_err("MonitorClientImpl not initialized...");
        return -1;
    }

    return MonitorClientImpl::instance().ping();
}

int MonitorClient::select_stat(event_cond_t& cond, event_select_t& stat) {

    if (unlikely(!MonitorClientImpl::instance().already_initialized_)) {
        log_err("MonitorClientImpl not initialized...");
        return -1;
    }

    return MonitorClientImpl::instance().select_stat(cond, stat);
}


int MonitorClient::select_stat(const std::string& metric, int64_t& count, int64_t& avg, time_t tm_intervel) {

    if (unlikely(!MonitorClientImpl::instance().already_initialized_)) {
        log_err("MonitorClientImpl not initialized...");
        return -1;
    }

    event_cond_t cond {};

    cond.version =  "1.0.0";
    cond.metric  = metric;
    cond.tm_interval = tm_intervel;
    cond.groupby = GroupType::kGroupNone;

    event_select_t stat {};
    if (MonitorClientImpl::instance().select_stat(cond, stat) != 0) {
        return -1;
    }

    count = stat.summary.count;
    avg = stat.summary.value_avg;

    return 0;
}

int MonitorClient::select_stat(const std::string& metric, const std::string& tag,
                               int64_t& count, int64_t& avg, time_t tm_intervel) {

    if (unlikely(!MonitorClientImpl::instance().already_initialized_)) {
        log_err("MonitorClientImpl not initialized...");
        return -1;
    }

    event_cond_t cond {};
    cond.version =  "1.0.0";
    cond.metric = metric;
    cond.tag = tag;
    cond.tm_interval = tm_intervel;
    cond.groupby = GroupType::kGroupNone;

    event_select_t stat {};
    if (MonitorClientImpl::instance().select_stat(cond, stat) != 0) {
        return -1;
    }

    count = stat.summary.count;
    avg = stat.summary.value_avg;

    return 0;
}

int MonitorClient::select_stat_groupby_tag(const std::string& metric,
                                           event_select_t& stat, time_t tm_intervel) {

    if (unlikely(!MonitorClientImpl::instance().already_initialized_)) {
        log_err("MonitorClientImpl not initialized...");
        return -1;
    }

    event_cond_t cond {};

    cond.version =  "1.0.0";
    cond.metric = metric;
    cond.tm_interval = tm_intervel;
    cond.groupby = GroupType::kGroupbyTag;

    return MonitorClientImpl::instance().select_stat(cond, stat);
}

int MonitorClient::select_stat_groupby_time(const std::string& metric,
                                            event_select_t& stat, time_t tm_intervel) {

    if (unlikely(!MonitorClientImpl::instance().already_initialized_)) {
        log_err("MonitorClientImpl not initialized...");
        return -1;
    }

    event_cond_t cond {};

    cond.version =  "1.0.0";
    cond.metric = metric;
    cond.tm_interval = tm_intervel;
    cond.groupby = GroupType::kGroupbyTimestamp;

    return MonitorClientImpl::instance().select_stat(cond, stat);
}


int MonitorClient::select_stat_groupby_time(const std::string& metric, const std::string& tag,
                                            event_select_t& stat, time_t tm_intervel) {

    if (unlikely(!MonitorClientImpl::instance().already_initialized_)) {
        log_err("MonitorClientImpl not initialized...");
        return -1;
    }

    event_cond_t cond {};

    cond.version =  "1.0.0";
    cond.metric = metric;
    cond.tag = tag;
    cond.tm_interval = tm_intervel;
    cond.groupby = GroupType::kGroupbyTimestamp;

    return MonitorClientImpl::instance().select_stat(cond, stat);
}


int MonitorClient::known_metrics(event_handler_conf_t& handler_conf, std::vector<std::string>& metrics, std::string service) {

    if (unlikely(!MonitorClientImpl::instance().already_initialized_)) {
        log_err("MonitorClientImpl not initialized...");
        return -1;
    }

    std::string version = "1.0.0";
    return MonitorClientImpl::instance().known_metrics(version, service, handler_conf, metrics);
}

int MonitorClient::known_services(std::vector<std::string>& services) {

    if (unlikely(!MonitorClientImpl::instance().already_initialized_)) {
        log_err("MonitorClientImpl not initialized...");
        return -1;
    }

    std::string version = "1.0.0";
    return MonitorClientImpl::instance().known_services(version, services);
}

int MonitorClient::update_runtime_conf(const libconfig::Config& conf) {

    if (unlikely(!MonitorClientImpl::instance().already_initialized_)) {
        log_err("MonitorClientImpl not initialized...");
        return -1;
    }

    return MonitorClientImpl::instance().update_runtime_conf(conf);
}

int MonitorClient::module_status(std::string& strModule, std::string& strKey, std::string& strValue) {

    if (unlikely(!MonitorClientImpl::instance().already_initialized_)) {
        log_err("MonitorClientImpl not initialized...");
        return -1;
    }

    return MonitorClientImpl::instance().module_status(strModule, strKey, strValue);
}

} // end namespace tzmonitor_client

