#include "General.h"
#include "Helper.h"
#include <utils/Log.h>

#include "Manager.h"

#include <connect/ConnPool.h>
#include <connect/SqlConn.h>
#include <connect/RedisConn.h>

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

const std::string& request_http_docu_root() {
    SAFE_ASSERT(Manager::instance().http_server_ptr_);
    return Manager::instance().http_server_ptr_->document_root();
}

const std::vector<std::string>& request_http_docu_index() {
    SAFE_ASSERT(Manager::instance().http_server_ptr_);
    return Manager::instance().http_server_ptr_->document_index();
}

} // end namespace
