#include "General.h"
#include "Helper.h"
#include <utils/Log.h>

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

} // end namespace
