#ifndef _TZ_HELPER_H_
#define _TZ_HELPER_H_

#include <sys/time.h>

#include <memory>
#include <string>
#include <vector>
#include <functional>

#include <boost/noncopyable.hpp>


// SQL connection pool
class SqlConn;
typedef std::shared_ptr<SqlConn> sql_conn_ptr;

// Redis connection pool
class RedisConn;
typedef std::shared_ptr<RedisConn> redis_conn_ptr;

// Timer Task helper
typedef std::function<void ()> TimerEventCallable;
class TimerService;



//
// free functions, in helper namespace

namespace helper {


bool request_scoped_sql_conn(sql_conn_ptr& conn);
sql_conn_ptr request_sql_conn();
sql_conn_ptr try_request_sql_conn(size_t msec);
void free_sql_conn(sql_conn_ptr conn);


bool request_scoped_redis_conn(redis_conn_ptr& conn);


std::shared_ptr<TimerService> request_timer_service();
int64_t register_timer_task(TimerEventCallable func, int64_t msec, bool persist = true, bool fast = true);
int64_t revoke_timer_task(int64_t index);


} // namespace

#endif // _TZ_HELPER_H_
