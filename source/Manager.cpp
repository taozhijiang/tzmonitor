#include <cstdlib>

#include <memory>
#include <string>
#include <map>

#include <connect/SqlConn.h>
#include <connect/RedisConn.h>
#include <httpd/HttpServer.h>

#include <module/TimerService.h>
#include <module/RedisData.h>

#include <thrifting/helper/TThreadedHelper.h>
#include <thrifting/helper/TThreadPoolHelper.h>
#include <thrifting/helper/TNonblockingHelper.h>

#include <thrifting/biz/TzMonitorService.h>

#include <core/EventRepos.h>

#include <utils/Utils.h>

#include <core/EventRepos.h>

#include "Helper.h"
#include "Manager.h"

// 在主线程中最先初始化，所以不考虑竞争条件问题
Manager& Manager::instance() {
    static Manager service;
    return service;
}

Manager::Manager():
    initialized_(false){
}


bool Manager::init() {

    if (initialized_) {
        log_err("Manager already initlialized...");
        return false;
    }

    (void)RedisData::instance();
    (void)EventRepos::instance();

    // Timer
    timer_service_ptr_.reset(new TimerService());
    if (!timer_service_ptr_ || !timer_service_ptr_->init()) {
        log_err("Init TimerService failed!");
        return false;
    }

    // MySQL
    std::string mysql_hostname;
    int mysql_port;
    std::string mysql_username;
    std::string mysql_passwd;
    std::string mysql_database;
    if (!get_config_value("mysql.host_addr", mysql_hostname) || !get_config_value("mysql.host_port", mysql_port) ||
        !get_config_value("mysql.username", mysql_username) || !get_config_value("mysql.passwd", mysql_passwd) ||
        !get_config_value("mysql.database", mysql_database)){
        log_err("Error, get mysql config value error");
        return false;
    }

    int conn_pool_size = 0;
    if (!get_config_value("mysql.conn_pool_size", conn_pool_size)) {
        conn_pool_size = 20;
        log_info("Using default conn_pool size: 20");
    }

    SqlConnPoolHelper helper(mysql_hostname, mysql_port,
                             mysql_username, mysql_passwd, mysql_database);
    sql_pool_ptr_.reset(new ConnPool<SqlConn, SqlConnPoolHelper>("MySQLPool", conn_pool_size, helper, 60 /*60s*/));
    if (!sql_pool_ptr_ || !sql_pool_ptr_->init()) {
        log_err("Init SqlConnPool failed!");
        return false;
    }

    // Redis
    std::string redis_hostname;
    int redis_port;
    std::string redis_passwd;
    if (!get_config_value("redis.host_addr", redis_hostname) || !get_config_value("redis.host_port", redis_port)){
        log_err("Error, get redis config value error");
        return false;
    }
    get_config_value("redis.passwd", redis_passwd);
    int redis_pool_size;
    if (!get_config_value("redis.conn_pool_size", redis_pool_size)) {
        redis_pool_size = 20;
        log_info("Using default conn_pool size: 20");
    }

    RedisConnPoolHelper redis_helper(redis_hostname, redis_port, redis_passwd);
    redis_pool_ptr_.reset(new ConnPool<RedisConn, RedisConnPoolHelper>("RedisPool", redis_pool_size, redis_helper, 60 /*60s*/));
    if (!redis_pool_ptr_ || !redis_pool_ptr_->init()) {
        log_err("Init RedisConnPool failed!");
        return false;
    }

    RedisData::instance().init();

    // Web
    std::string bind_addr;
    int listen_port = 0;
    if (!get_config_value("http.bind_addr", bind_addr) || !get_config_value("http.listen_port", listen_port) ){
        log_err("Error, get value error");
        return false;
    }

    int thread_pool_size = 0;
    if (!get_config_value("http.thread_pool_size", thread_pool_size)) {
        thread_pool_size = 8;
        log_info("Using default thread_pool size: 8");
    }
    log_info("listen at: %s:%d, thread_pool: %d", bind_addr.c_str(), listen_port, thread_pool_size);
    http_server_ptr_.reset(new HttpServer(bind_addr, static_cast<unsigned short>(listen_port), thread_pool_size));
    if (!http_server_ptr_ || !http_server_ptr_->init()) {
        log_err("Init HttpServer failed!");
        return false;
    }

    // Thrift
    int thrift_port, thrift_thread_size, thrift_io_thread_size;
    if (!get_config_value("thrift.listen_port", thrift_port) ||
        !get_config_value("thrift.thread_size", thrift_thread_size) ||
        !get_config_value("thrift.io_thread_size", thrift_io_thread_size) ){
        log_err("Error, get value error");
        return false;
    }

    // TThreaded
//    monitor_service_ptr_.reset(new TzMonitorService<TThreadedHelper>(thrift_port));
    // TThreadPool
//    monitor_service_ptr_.reset(new TzMonitorService<TThreadPoolHelper>(thrift_port, thrift_thread_size));
    // TNonblocking
    monitor_service_ptr_.reset(new TzMonitorService<TNonblockingHelper>(thrift_port, thrift_thread_size, thrift_io_thread_size));

    if (!monitor_service_ptr_ || !monitor_service_ptr_->init()) {
        log_err("Init MonitorService failed!");
        return false;
    }

    // Core Biz
    if (!EventRepos::instance().init()) {
        log_err("Init EventRepos failed!");
        return false;
    }


    // start work
    timer_service_ptr_->start_timer();
    http_server_ptr_->io_service_threads_.start_threads();

    // do real service
    http_server_ptr_->service();
    monitor_service_ptr_->start_service();

    log_info("Manager all initialized...");
    initialized_ = true;

    return true;
}


bool Manager::service_graceful() {

    http_server_ptr_->io_service_stop_graceful();
    timer_service_ptr_->stop_graceful();
    log_info("timer_service_ graceful finished!");

    return true;
}

void Manager::service_terminate() {
    ::sleep(1);
    ::_exit(0);
}

bool Manager::service_joinall() {

    http_server_ptr_->io_service_join();
    timer_service_ptr_->join();

    return true;
}
