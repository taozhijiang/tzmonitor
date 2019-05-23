/*-
 * Copyright (c) 2019 TAO Zhijiang<taozhijiang@gmail.com>
 *
 * Licensed under the BSD-3-Clause license, see LICENSE for full information.
 *
 */


// 此库的作用就是对业务服务产生的数据进行压缩，然后开辟
// 独立的线程向服务端进行提交，以避免多次独立提交带来的额外消耗

// 如果支持新标准，future更加合适做这个事情

// 客户端使用，尽量减少依赖的库

#include <xtra_rhel.h>

#include <unistd.h>

#include <cassert>
#include <sstream>
#include <thread>
#include <functional>

#include <container/EQueue.h>
#include <other/Log.h>

#include <Client/RpcClient.h>
#include <Client/include/HeraclesClient.h>

#include <Client/MonitorRpcClientHelper.h>

#include <Business/Sort.h>

using tzrpc_client::MonitorRpcClientHelper;

namespace heracles_client {


struct HeraclesClientConf {

    bool report_enabled_;

    int  report_queue_limit_;
    int  size_per_report_;

    HeraclesClientConf() :
        report_enabled_(true),
        report_queue_limit_(0),
        size_per_report_(5000) {
    }
} __attribute__((aligned(4)));

class HeraclesClientImpl {

    friend class HeraclesClient;

    __noncopyable__(HeraclesClientImpl)

private:

    bool init();
    bool init(const std::string& cfgFile);
    bool init(const libconfig::Setting& setting);
    bool init(const std::string& service, const std::string& entity_idx,
              const std::string& addr, uint16_t port);

    int ping();
    int report_event(const std::string& metric, int64_t value, const std::string& tag);
    int select_stat(event_cond_t& cond, event_select_t& stat);

    int known_metrics(const std::string& version, const std::string& service,
                      event_handler_conf_t& handler_conf, std::vector<std::string>& metrics);
    int known_services(const std::string& version, std::vector<std::string>& services);

    int module_runtime(const libconfig::Config& conf);
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
            roo::log_err("MonitorRpcClientHelper not initialized, fatal!");
            return -1;
        }

        auto code = client_agent_->rpc_event_submit(*report_ptr);
        if (code == 0) {
            roo::log_info("report submit ok.");
            return 0;
        }

        roo::log_err("report submit return code: %d", code);
        return code;
    }

    // 新的线程创建新的提交连接，否则并没有增加并发量
    int do_additional_report(std::shared_ptr<MonitorRpcClientHelper>& client_agent, event_report_ptr_t report_ptr) {

        if (!report_ptr || !client_agent) {
            return -1;
        }

        auto code = client_agent->rpc_event_submit(*report_ptr);
        if (code == 0) {
            roo::log_info("report submit ok.");
            return 0;
        }

        roo::log_err("report submit return code: %d", code);
        return code;
    }

private:
    // 确保 service和entity_idx已经是定义良好的了
    HeraclesClientImpl(std::string service, std::string entity_idx) :
        client_agent_(),
        thread_terminate_(false),
        thread_run_(),
        lock_(),
        msgid_(0), current_time_(0),
        current_slot_(),
        submit_queue_(),
        already_initialized_(false),
        service_(service), entity_idx_(entity_idx),
        monitor_addr_(),
        monitor_port_(),
        conf_(),
        cfgFile_()   {
    }

    ~HeraclesClientImpl() {
        thread_terminate_ = true;
        if (thread_run_ && thread_run_->joinable()) {
            thread_run_->join();
        }
    }

    static HeraclesClientImpl& instance();

private:

    std::shared_ptr<MonitorRpcClientHelper> client_agent_;

    // 默认开启一个提交，当发现待提交队列过长的时候，自动开辟future任务
    bool thread_terminate_;
    std::shared_ptr<std::thread> thread_run_;
    void run();


    // used int report procedure
    std::mutex lock_;

    int64_t msgid_;
    time_t  current_time_;
    std::vector<event_data_t> current_slot_;

    roo::EQueue<event_report_ptr_t> submit_queue_;

    // 该单例只允许初始化一次
    bool already_initialized_;
    std::string service_;
    std::string entity_idx_;
    std::string monitor_addr_;
    uint16_t    monitor_port_;

    HeraclesClientConf conf_;

    std::string cfgFile_;
};


// Impl member function

HeraclesClientImpl& HeraclesClientImpl::instance() {
    static HeraclesClientImpl helper(program_invocation_short_name, "");
    return helper;
}

bool HeraclesClientImpl::init(const std::string& cfgFile) {

    libconfig::Config cfg;
    try {

        cfgFile_ = cfgFile;
        cfg.readFile(cfgFile.c_str());

        const libconfig::Setting& setting = cfg.lookup("rpc.monitor_client");
        return init(setting);

    } catch (libconfig::FileIOException& fioex) {
        roo::log_err("I/O error while reading file: %s", cfgFile.c_str());
        return false;
    } catch (libconfig::ParseException& pex) {
        roo::log_err("Parse error at %d - %s", pex.getLine(), pex.getError());
        return false;
    } catch (...) {
        roo::log_err("process cfg failed.");
    }

    return false;
}

bool HeraclesClientImpl::init() {

    if (cfgFile_.empty()) {
        roo::log_err("cfgFile_ not initialized...");
        return false;
    }

    return init(cfgFile_);
}


bool HeraclesClientImpl::init(const libconfig::Setting& setting) {

    std::string serv_addr;
    int serv_port = 0;
    if (!setting.lookupValue("serv_addr", serv_addr) ||
        !setting.lookupValue("serv_port", serv_port) ||
        serv_addr.empty() || serv_port <= 0) {
        roo::log_err("get rpc server addr config failed.");
        return false;
    }

    // 合法的值才会覆盖默认值
    bool value_b;
    int  value_i;

    if (setting.lookupValue("report_enabled", value_b)) {
        roo::log_warning("update report_enabled from %s to %s",
                         (conf_.report_enabled_ ? "on" : "off"), (value_b ? "on" : "off"));
        conf_.report_enabled_ = value_b;
    }

    if (setting.lookupValue("report_queue_limit", value_i) && value_i >= 0) {
        roo::log_warning("update report_queue_limit from %d to %d",
                         conf_.report_queue_limit_, value_i);
        conf_.report_queue_limit_ = value_i;
    }

    if (setting.lookupValue("size_per_report", value_i) && value_i > 0) {
        roo::log_warning("update size_per_report from %d to %d",
                         conf_.size_per_report_, value_i);
        conf_.size_per_report_ = value_i;
    }

    std::string service;
    std::string entity_idx;
    setting.lookupValue("service", service);
    setting.lookupValue("entity_idx", entity_idx);

    // load other conf

    return init(service, entity_idx, serv_addr, serv_port);
}


bool HeraclesClientImpl::init(const std::string& service, const std::string& entity_idx,
                              const std::string& addr, uint16_t port) {


    std::lock_guard<std::mutex> lock(lock_);

    if (already_initialized_) {
        roo::log_err("HeraclesClientImpl already successfully initialized...");
        return true;
    }

    if (!service.empty()) {
        roo::log_warning("override service from %s to %s", service_.c_str(), service.c_str());
        service_ = service;
    }

    if (!entity_idx.empty()) {
        roo::log_warning("override entity_idx from %s to %s", entity_idx_.c_str(), entity_idx.c_str());
        entity_idx_ = entity_idx;
    }

    monitor_addr_ = addr;
    monitor_port_ = port;

    if (service_.empty() || monitor_addr_.empty() || monitor_port_ == 0) {
        roo::log_err("critical param error: service %s, addr %s, port %u",
                     service_.c_str(), monitor_addr_.c_str(), monitor_port_);
        return false;
    }

    client_agent_ = std::make_shared<MonitorRpcClientHelper>(monitor_addr_, monitor_port_);
    if (!client_agent_) {
        roo::log_err("not available agent found!");
        return false;
    }

    if (client_agent_->rpc_ping() != 0) {
        roo::log_err("client agent ping test failed...");
        return false;
    }

    thread_run_.reset(new std::thread(std::bind(&HeraclesClientImpl::run, this)));
    if (!thread_run_) {
        roo::log_err("create run work thread failed! ");
        return false;
    }

    roo::log_info("HeraclesClientImpl init ok!");
    already_initialized_ = true;

    return true;
}

// rpc.monitor_client
int HeraclesClientImpl::module_runtime(const libconfig::Config& conf) {

    try {

        // initialize client conf
        const libconfig::Setting& setting = conf.lookup("rpc.monitor_client");

        // conf update
        bool value_b;
        int  value_i;

        if (setting.lookupValue("report_enabled", value_b)) {
            roo::log_warning("update report_enabled from %s to %s",
                             (conf_.report_enabled_ ? "on" : "off"), (value_b ? "on" : "off"));
            conf_.report_enabled_ = value_b;
        }

        if (setting.lookupValue("report_queue_limit", value_i) && value_i >= 0) {
            roo::log_warning("update report_queue_limit from %d to %d",
                             conf_.report_queue_limit_, value_i);
            conf_.report_queue_limit_ = value_i;
        }

        if (setting.lookupValue("size_per_report", value_i) && value_i > 0) {
            roo::log_warning("update size_per_report from %d to %d",
                             conf_.size_per_report_, value_i);
            conf_.size_per_report_ = value_i;
        }

        return 0;

    } catch (const libconfig::SettingNotFoundException& nfex) {
        roo::log_err("rpc.monitor_client not found!");
    } catch (std::exception& e) {
        roo::log_err("execptions catched for %s",  e.what());
    }

    return -1;
}

int HeraclesClientImpl::module_status(std::string& module, std::string& name, std::string& val) {

    module = "heracles_client";
    // service + entity_idx

    name = service_;
    if (!entity_idx_.empty()) {
        name += "!";
        name += entity_idx_;
    }

    std::stringstream ss;

    ss << "\t" << "service: " << service_ << std::endl;
    ss << "\t" << "entity_idx: " << entity_idx_ << std::endl;

    ss << "\t" << "report_enable: " << (conf_.report_enabled_ ? "true" : "false") << std::endl;
    ss << "\t" << "size_per_report: " << conf_.size_per_report_ << std::endl;
    ss << "\t" << "report_queue_limit: " << conf_.report_queue_limit_ << std::endl;
    ss << "\t" << "current_queue: " << submit_queue_.SIZE() << std::endl;

    val = ss.str();

    return 0;
}


int HeraclesClientImpl::ping() {

    if (!client_agent_) {
        roo::log_err("MonitorRpcClientHelper not initialized, fatal!");
        return -1;
    }

    auto code = client_agent_->rpc_ping();
    if (code == 0) {
        roo::log_info("ping test ok.");
        return 0;
    }

    roo::log_err("ping return code: %d", code);
    return code;
}


int HeraclesClientImpl::report_event(const std::string& metric, int64_t value, const std::string& tag) {

    if (!conf_.report_enabled_) {
        return 0;
    }

    std::lock_guard<std::mutex> lock(lock_);

    event_data_t item{};
    item.metric = metric;
    item.value = value;
    item.tag = tag;
    item.msgid = ++msgid_;

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

        item.msgid = ++msgid_; // 新时间，新起点
    }

    // item.name 可能是空的，我们会定期插入空的消息，强制没有满的消息刷新提交出去
    // 空消息就是metric为0的消息
    if (likely(!item.metric.empty())) {
        current_slot_.emplace_back(item);
    }

    return 0;
}

int HeraclesClientImpl::select_stat(event_cond_t& cond, event_select_t& stat) {

    if (!client_agent_) {
        roo::log_err("MonitorRpcClientHelper not initialized, fatal!");
        return -1;
    }

    // 如果为空才覆盖
    if (cond.service.empty()) {
        cond.service = service_;
    }
    auto code = client_agent_->rpc_event_select(cond, stat);
    if (code != 0) {
        roo::log_err("event select return code: %d", code);
        return code;
    }

    // 是否进行排序
    // 如果cond.limit == 0，则服务端的记录会全部返回的，就不需要在服务
    // 端进行排序，在客户端执行排序操作
    if (cond.orderby != OrderByType::kOrderByNone &&
        cond.limit == 0 &&
        !stat.info.empty()) {
        roo::log_info("we will order result set manualy, groupby: %d, orderby: %d",
                      cond.groupby, cond.orderby);

        Sort::do_sort(stat.info, cond.orderby, cond.orders);
    }

    roo::log_info("event select ok.");
    return 0;
}


int HeraclesClientImpl::known_metrics(const std::string& version, const std::string& service,
                                      event_handler_conf_t& handler_conf, std::vector<std::string>& metrics) {

    if (!client_agent_) {
        roo::log_err("MonitorRpcClientHelper not initialized, fatal!");
        return -1;
    }

    std::string service_t = service_;
    if (!service.empty()) {
        service_t = service;
    }
    auto code = client_agent_->rpc_known_metrics(version, service_t, handler_conf, metrics);
    if (code == 0) {
        roo::log_info("known metrics ok.");
        return 0;
    }

    roo::log_err("known metrics return code: %d", code);
    return code;
}

int HeraclesClientImpl::known_services(const std::string& version, std::vector<std::string>& services) {

    if (!client_agent_) {
        roo::log_err("MonitorRpcClientHelper not initialized, fatal!");
        return -1;
    }

    auto code = client_agent_->rpc_known_services(version, services);
    if (code == 0) {
        roo::log_info("known services ok.");
        return 0;
    }

    roo::log_err("known services return code: %d", code);
    return code;
}



void HeraclesClientImpl::run() {

    roo::log_info("HeraclesClient submit thread %#lx begin to run ...", (long)pthread_self());

    while (true) {

        if (thread_terminate_) {
            roo::log_info("HeraclesClient submit thread %#lx about to terminate ...", (long)pthread_self());
            break;
        }

        event_report_ptr_t report_ptr;
        time_t start = ::time(NULL);
        if (!submit_queue_.POP(report_ptr, 1000)) {
            report_empty_event(); // 触发事件提交
            continue;
        }

        do_report(report_ptr);
        if (::time(NULL) != start) {
            report_empty_event(); // 触发事件提交
        }

        if (conf_.report_queue_limit_ != 0 && submit_queue_.SIZE() > conf_.report_queue_limit_) {
            roo::log_err("about to shrink submit_queue, current %lu, limit %d",
                         submit_queue_.SIZE(), conf_.report_queue_limit_);
            submit_queue_.SHRINK_FRONT(conf_.report_queue_limit_);
        }
    }
}


// call forward
HeraclesClient::HeraclesClient(std::string entity_idx) {
    (void)HeraclesClientImpl::instance();
}

HeraclesClient::HeraclesClient(std::string service, std::string entity_idx) {
    (void)HeraclesClientImpl::instance();
}

HeraclesClient::~HeraclesClient() { }

bool HeraclesClient::init() {
    return HeraclesClientImpl::instance().init();
}

bool HeraclesClient::init(const std::string& cfgFile) {
    return HeraclesClientImpl::instance().init(cfgFile);
}

bool HeraclesClient::init(const libconfig::Setting& setting) {
    return HeraclesClientImpl::instance().init(setting);
}

bool HeraclesClient::init(const std::string& addr,  uint16_t port) {
    return HeraclesClientImpl::instance().init("", "", addr, port);
}

bool HeraclesClient::init(const std::string& service, const std::string& entity_idx,
                          const std::string& addr, uint16_t port) {
    return HeraclesClientImpl::instance().init(service, entity_idx, addr, port);
}


// 初始化检查提前做，将开销分担到各个调用线程中去。

int HeraclesClient::report_event(const std::string& name, int64_t value, std::string flag) {

    if (unlikely(!HeraclesClientImpl::instance().already_initialized_)) {
        roo::log_err("HeraclesClientImpl not initialized...");
        return -1;
    }

    return HeraclesClientImpl::instance().report_event(name, value, flag);
}

int HeraclesClient::ping() {

    if (unlikely(!HeraclesClientImpl::instance().already_initialized_)) {
        roo::log_err("HeraclesClientImpl not initialized...");
        return -1;
    }

    return HeraclesClientImpl::instance().ping();
}

int HeraclesClient::select_stat(event_cond_t& cond, event_select_t& stat) {

    if (unlikely(!HeraclesClientImpl::instance().already_initialized_)) {
        roo::log_err("HeraclesClientImpl not initialized...");
        return -1;
    }

    return HeraclesClientImpl::instance().select_stat(cond, stat);
}


int HeraclesClient::select_stat(const std::string& metric, int64_t& count, int64_t& avg, time_t tm_intervel) {

    if (unlikely(!HeraclesClientImpl::instance().already_initialized_)) {
        roo::log_err("HeraclesClientImpl not initialized...");
        return -1;
    }

    event_cond_t cond{};

    cond.version =  "1.0.0";
    cond.metric  = metric;
    cond.tm_interval = tm_intervel;
    cond.groupby = GroupType::kGroupNone;

    event_select_t stat{};
    if (HeraclesClientImpl::instance().select_stat(cond, stat) != 0) {
        return -1;
    }

    count = stat.summary.count;
    avg = stat.summary.value_avg;

    return 0;
}

int HeraclesClient::select_stat(const std::string& metric, const std::string& tag,
                                int64_t& count, int64_t& avg, time_t tm_intervel) {

    if (unlikely(!HeraclesClientImpl::instance().already_initialized_)) {
        roo::log_err("HeraclesClientImpl not initialized...");
        return -1;
    }

    event_cond_t cond{};
    cond.version =  "1.0.0";
    cond.metric = metric;
    cond.tag = tag;
    cond.tm_interval = tm_intervel;
    cond.groupby = GroupType::kGroupNone;

    event_select_t stat{};
    if (HeraclesClientImpl::instance().select_stat(cond, stat) != 0) {
        return -1;
    }

    count = stat.summary.count;
    avg = stat.summary.value_avg;

    return 0;
}

int HeraclesClient::select_stat_groupby_tag(const std::string& metric,
                                            event_select_t& stat, time_t tm_intervel) {

    if (unlikely(!HeraclesClientImpl::instance().already_initialized_)) {
        roo::log_err("HeraclesClientImpl not initialized...");
        return -1;
    }

    event_cond_t cond{};

    cond.version =  "1.0.0";
    cond.metric = metric;
    cond.tm_interval = tm_intervel;
    cond.groupby = GroupType::kGroupbyTag;

    return HeraclesClientImpl::instance().select_stat(cond, stat);
}

int HeraclesClient::select_stat_groupby_time(const std::string& metric,
                                             event_select_t& stat, time_t tm_intervel) {

    if (unlikely(!HeraclesClientImpl::instance().already_initialized_)) {
        roo::log_err("HeraclesClientImpl not initialized...");
        return -1;
    }

    event_cond_t cond{};

    cond.version =  "1.0.0";
    cond.metric = metric;
    cond.tm_interval = tm_intervel;
    cond.groupby = GroupType::kGroupbyTimestamp;

    return HeraclesClientImpl::instance().select_stat(cond, stat);
}


int HeraclesClient::select_stat_groupby_time(const std::string& metric, const std::string& tag,
                                             event_select_t& stat, time_t tm_intervel) {

    if (unlikely(!HeraclesClientImpl::instance().already_initialized_)) {
        roo::log_err("HeraclesClientImpl not initialized...");
        return -1;
    }

    event_cond_t cond{};

    cond.version =  "1.0.0";
    cond.metric = metric;
    cond.tag = tag;
    cond.tm_interval = tm_intervel;
    cond.groupby = GroupType::kGroupbyTimestamp;

    return HeraclesClientImpl::instance().select_stat(cond, stat);
}


int HeraclesClient::select_stat_groupby_tag_ordered(const std::string& metric, const order_cond_t& order,
                                                    event_select_t& stat, time_t tm_intervel) {

    if (unlikely(!HeraclesClientImpl::instance().already_initialized_)) {
        roo::log_err("HeraclesClientImpl not initialized...");
        return -1;
    }

    // more param check add later

    // timestamp, tag, count, sum, avg, min, max, p10, p50, p90
    if (order.limit_ < 0) {
        roo::log_err("invalid param: %d", order.limit_);
        return -1;
    }

    event_cond_t cond{};

    cond.version =  "1.0.0";
    cond.metric = metric;
    cond.tm_interval = tm_intervel;
    cond.groupby = GroupType::kGroupbyTag;

    cond.orderby = order.orderby_;
    cond.orders = order.orders_;
    cond.limit = order.limit_;

    return HeraclesClientImpl::instance().select_stat(cond, stat);
}

int HeraclesClient::select_stat_groupby_time_ordered(const std::string& metric, const order_cond_t& order,
                                                     event_select_t& stat, time_t tm_intervel) {

    if (unlikely(!HeraclesClientImpl::instance().already_initialized_)) {
        roo::log_err("HeraclesClientImpl not initialized...");
        return -1;
    }


    // more param check add later

    // timestamp, tag, count, sum, avg, min, max, p10, p50, p90
    if (order.limit_ < 0) {
        roo::log_err("invalid param: %d", order.limit_);
        return -1;
    }


    event_cond_t cond{};

    cond.version =  "1.0.0";
    cond.metric = metric;
    cond.tm_interval = tm_intervel;
    cond.groupby = GroupType::kGroupbyTimestamp;

    cond.orderby = order.orderby_;
    cond.orders = order.orders_;
    cond.limit = order.limit_;

    return HeraclesClientImpl::instance().select_stat(cond, stat);
}

int HeraclesClient::select_stat_groupby_time_ordered(const std::string& metric, const std::string& tag,
                                                     const order_cond_t& order, event_select_t& stat, time_t tm_intervel) {

    if (unlikely(!HeraclesClientImpl::instance().already_initialized_)) {
        roo::log_err("HeraclesClientImpl not initialized...");
        return -1;
    }


    // more param check add later

    // timestamp, tag, count, sum, avg, min, max, p10, p50, p90
    if (order.limit_ < 0) {
        roo::log_err("invalid param: %d", order.limit_);
        return -1;
    }


    event_cond_t cond{};

    cond.version =  "1.0.0";
    cond.metric = metric;
    cond.tag = tag;
    cond.tm_interval = tm_intervel;
    cond.groupby = GroupType::kGroupbyTimestamp;

    cond.orderby = order.orderby_;
    cond.orders = order.orders_;
    cond.limit = order.limit_;

    return HeraclesClientImpl::instance().select_stat(cond, stat);
}

int HeraclesClient::known_metrics(event_handler_conf_t& handler_conf, std::vector<std::string>& metrics, std::string service) {

    if (unlikely(!HeraclesClientImpl::instance().already_initialized_)) {
        roo::log_err("HeraclesClientImpl not initialized...");
        return -1;
    }

    std::string version = "1.0.0";
    return HeraclesClientImpl::instance().known_metrics(version, service, handler_conf, metrics);
}

int HeraclesClient::known_services(std::vector<std::string>& services) {

    if (unlikely(!HeraclesClientImpl::instance().already_initialized_)) {
        roo::log_err("HeraclesClientImpl not initialized...");
        return -1;
    }

    std::string version = "1.0.0";
    return HeraclesClientImpl::instance().known_services(version, services);
}

int HeraclesClient::module_runtime(const libconfig::Config& conf) {

    if (unlikely(!HeraclesClientImpl::instance().already_initialized_)) {
        roo::log_err("HeraclesClientImpl not initialized...");
        return -1;
    }

    return HeraclesClientImpl::instance().module_runtime(conf);
}

int HeraclesClient::module_status(std::string& strModule, std::string& strKey, std::string& strValue) {

    if (unlikely(!HeraclesClientImpl::instance().already_initialized_)) {
        roo::log_err("HeraclesClientImpl not initialized...");
        return -1;
    }

    return HeraclesClientImpl::instance().module_status(strModule, strKey, strValue);
}

} // end namespace heracles_client

