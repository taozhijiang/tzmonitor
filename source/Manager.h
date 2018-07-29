/*-
 * Copyright (c) 2018 TAO Zhijiang<taozhijiang@gmail.com>
 *
 * Licensed under the BSD-3-Clause license, see LICENSE for full information.
 *
 */


#ifndef __TZ_MANAGER_H__
#define __TZ_MANAGER_H__

#include <string>
#include <map>
#include <vector>

namespace tzhttpd {
class HttpServer;
}

class TimerService;

template <typename T, typename Helper>
class ConnPool;

class SqlConn;
class SqlConnPoolHelper;

class RedisConn;
class RedisConnPoolHelper;

template <typename ServiceHandler, typename ServiceProcessor>
class TThreadedHelper;

template <typename ServiceHandler, typename ServiceProcessor>
class TThreadPoolHelper;

template <typename ServiceHandler, typename ServiceProcessor>
class TNonblockingHelper;


template < template <typename, typename> class ServerHelperType>
class TzMonitorService;


class Manager {
public:
    static Manager& instance();

public:
    bool init();

    bool service_joinall();
    bool service_graceful();
    void service_terminate();

private:
    Manager();

    bool initialized_;

public:
    std::shared_ptr<TimerService> timer_service_ptr_;
    std::shared_ptr<tzhttpd::HttpServer> http_server_ptr_;
    std::shared_ptr<ConnPool<SqlConn, SqlConnPoolHelper>> sql_pool_ptr_;
    std::shared_ptr<ConnPool<RedisConn, RedisConnPoolHelper>> redis_pool_ptr_;

//    std::shared_ptr<TzMonitorService<TThreadedHelper>>    monitor_service_ptr_;
//    std::shared_ptr<TzMonitorService<TThreadPoolHelper>>  monitor_service_ptr_;
    std::shared_ptr<TzMonitorService<TNonblockingHelper>> monitor_service_ptr_;
};


#endif //__TZ_MANAGER_H__
