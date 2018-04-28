#include <execinfo.h>

#include "General.h"
#include "Utils.h"
#include <utils/Log.h>

static void backtrace_info(int sig, siginfo_t *info, void *f) {
    int j, nptrs;
#define BT_SIZE 100
    char **strings;
    void *buffer[BT_SIZE];

    fprintf(stderr,       "\nSignal [%d] received.\n", sig);
    fprintf(stderr,       "======== Stack trace ========");

    nptrs = ::backtrace(buffer, BT_SIZE);
    fprintf(stderr,       "backtrace() returned %d addresses\n", nptrs);

    strings = ::backtrace_symbols(buffer, nptrs);
    if (strings == NULL) {
        perror("backtrace_symbols");
        exit(EXIT_FAILURE);
    }

    for (j = 0; j < nptrs; j++)
        fprintf(stderr, "%s\n", strings[j]);

    free(strings);

    fprintf(stderr,       "Stack Done!\n");

    ::kill(getpid(), sig);
    ::abort();

#undef BT_SIZE
}

void backtrace_init() {

    struct sigaction act;
    sigemptyset(&act.sa_mask);
    act.sa_flags     = SA_NODEFER | SA_ONSTACK | SA_RESETHAND | SA_SIGINFO;
    act.sa_sigaction = backtrace_info;
    sigaction(SIGABRT, &act, NULL);
    sigaction(SIGBUS,  &act, NULL);
    sigaction(SIGFPE,  &act, NULL);
    sigaction(SIGSEGV, &act, NULL);

    return;
}


#include <unistd.h>
#include <fcntl.h>

int set_nonblocking(int fd) {
    int flags = 0;

    flags = fcntl (fd, F_GETFL, 0);
    flags |= O_NONBLOCK;
    fcntl (fd, F_SETFL, flags);

    return 0;
}


libconfig::Config& get_config_object() {
    static libconfig::Config cfg;
    return cfg;
}

bool sys_config_init(const std::string& config_file) {

    libconfig::Config& cfg = get_config_object();

    try {
        cfg.readFile(config_file.c_str());
    } catch(libconfig::FileIOException &fioex) {
        log_err("I/O error while reading file.");
        return false;
    } catch(libconfig::ParseException &pex) {
        log_err("Parse error at %d - %s", pex.getLine(), pex.getError());
        return false;
    }

    return true;
}

#include "Manager.h"

#include <connect/ConnPool.h>
#include <connect/SqlConn.h>
#include <connect/RedisConn.h>

#include <boost/algorithm/string.hpp>
#include <connect/MqConn.h>

#include <module/TimerService.h>

#include <httpd/HttpServer.h>

namespace helper {

bool request_scoped_sql_conn(sql_conn_ptr& conn) {
    SAFE_ASSERT(Manager::instance().sql_pool_ptr_);
    return Manager::instance().sql_pool_ptr_->request_scoped_conn(conn);
}

sql_conn_ptr request_sql_conn() {
    SAFE_ASSERT(Manager::instance().sql_pool_ptr_);
    return Manager::instance().sql_pool_ptr_->request_conn();
}

sql_conn_ptr try_request_sql_conn(size_t msec) {
    SAFE_ASSERT(Manager::instance().sql_pool_ptr_);
    return Manager::instance().sql_pool_ptr_->try_request_conn(msec);
}

void free_sql_conn(sql_conn_ptr conn) {
    SAFE_ASSERT(Manager::instance().sql_pool_ptr_);
    return Manager::instance().sql_pool_ptr_->free_conn(conn);
}

bool request_scoped_redis_conn(redis_conn_ptr& conn) {
    SAFE_ASSERT(Manager::instance().redis_pool_ptr_);
    return Manager::instance().redis_pool_ptr_->request_scoped_conn(conn);
}

std::shared_ptr<TimerService> request_timer_service() {
    SAFE_ASSERT(Manager::instance().timer_service_ptr_);
    return Manager::instance().timer_service_ptr_;
}

int64_t register_timer_task(TimerEventCallable func, int64_t msec, bool persist, bool fast) {
    SAFE_ASSERT(Manager::instance().timer_service_ptr_);
    return Manager::instance().timer_service_ptr_->register_timer_task(func, msec, persist, fast);
}

int64_t revoke_timer_task(int64_t index) {
    SAFE_ASSERT(Manager::instance().timer_service_ptr_);
    return Manager::instance().timer_service_ptr_->revoke_timer_task(index);
}


int tz_mq_test() {

    std::shared_ptr<ConnPool<MqConn, MqConnPoolHelper>> mq_pool_ptr_;

    std::string connects = "amqp://tibank:%s@127.0.0.1:5672/tibank";
    std::string passwd   = "paybank";

    std::vector<std::string> vec;
    std::vector<std::string> realVec;
    boost::split(vec, connects, boost::is_any_of(";"));
    for (std::vector<std::string>::iterator it = vec.begin(); it != vec.cend(); ++it){
        std::string tmp = boost::trim_copy(*it);
        if (tmp.empty())
            continue;

        char connBuf[2048] = { 0, };
        snprintf(connBuf, sizeof(connBuf), tmp.c_str(), passwd.c_str());
        realVec.push_back(connBuf);
    }

    MqConnPoolHelper mq_helper(realVec, "paybank_exchange", "mqpooltest", "mqtest");
    mq_pool_ptr_.reset(new ConnPool<MqConn, MqConnPoolHelper>("MqPool", 7, mq_helper, 5*60 /*5min*/));
    if (!mq_pool_ptr_ || !mq_pool_ptr_->init()) {
        log_err("Init RabbitMqConnPool failed!");
        return false;
    }

    std::string msg = "TAOZJFSFiMSG";
    std::string outmsg;

    mq_conn_ptr conn;
    mq_pool_ptr_->request_scoped_conn(conn);
    if (!conn) {
        log_err("Request conn failed!");
        return -1;
    }

    int ret = conn->publish(msg);
    if (ret != 0) {
        log_err("publish message failed!");
        return -1;
    } else {
        log_info("Publish message ok!");
    }

    ret = conn->get(outmsg);
    if (ret != 0) {
        log_err("get message failed!");
        return -1;
    } else {
        log_err("get msg out : %s", outmsg.c_str());
    }


    msg = "MSG233333";
    ret = conn->publish(msg);
    if (ret != 0) {
        log_err("publish message2 failed!");
        return -1;
    } else {
        log_info("Publish message2 ok!");
    }

    ret = conn->consume(outmsg, 5);
    if (ret != 0) {
        log_err("consume message2 failed!");
        return -1;
    } else {
        log_err("consume msg2 out : %s", outmsg.c_str());
    }

    return 0;
}


const std::string& request_http_docu_root() {
    SAFE_ASSERT(Manager::instance().http_server_ptr_);
    return Manager::instance().http_server_ptr_->document_root();
}

const std::vector<std::string>& request_http_docu_index() {
    SAFE_ASSERT(Manager::instance().http_server_ptr_);
    return Manager::instance().http_server_ptr_->document_index();
}

void COUNT_FUNC_PERF::display_info(const std::string& env, int64_t time_ms, int64_t time_us) {
    log_debug("%s, %s perf: %ld.%ld ms", env_.c_str(), key_.c_str(), time_ms, time_us);
}

} // end namespace



namespace boost {

void assertion_failed(char const * expr, char const * function, char const * file, long line) {
    log_err("BAD!!! expr `%s` assert failed at %s(%ld): %s", expr, file, line, function);
}

}
