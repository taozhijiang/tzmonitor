#ifndef _TZ_LOG_H_
#define _TZ_LOG_H_

#include <stdarg.h>
#include <cstring>
#include <cstddef>

#include <algorithm>

#include <boost/current_function.hpp>

// LOG_EMERG   0   system is unusable
// LOG_ALERT   1   action must be taken immediately
// LOG_CRIT    2   critical conditions
// LOG_ERR     3   error conditions
// LOG_WARNING 4   warning conditions
// LOG_NOTICE  5   normal, but significant, condition
// LOG_INFO    6   informational message
// LOG_DEBUG   7   debug-level message

static const std::size_t MAX_LOG_BUF_SIZE = (16*1024 -2);

#if !defined(TZMONITOR_CLIENT) && !defined(BOOST_TEST_MODULE)

// man 3 syslog
#include <syslog.h>

class Log {
public:
    static Log& instance();
    bool init(int log_level);

    ~Log() {
        closelog();
    }

private:
    int get_time_prefix(char *buf, int size);

public:
    void log_api(int priority, const char *file, int line, const char *func, const char *msg, ...);
};

#define log_emerg(...)  Log::instance().log_api( LOG_EMERG, __FILE__, __LINE__, __FUNCTION__, __VA_ARGS__)
#define log_alert(...)  Log::instance().log_api( LOG_ALERT, __FILE__, __LINE__, __FUNCTION__, __VA_ARGS__)
#define log_crit(...)   Log::instance().log_api( LOG_CRIT, __FILE__, __LINE__, __FUNCTION__, __VA_ARGS__)
#define log_err(...)  Log::instance().log_api( LOG_ERR, __FILE__, __LINE__, __FUNCTION__, __VA_ARGS__)
#define log_warning(...)  Log::instance().log_api( LOG_WARNING, __FILE__, __LINE__, __FUNCTION__, __VA_ARGS__)
#define log_notice(...)  Log::instance().log_api( LOG_NOTICE, __FILE__, __LINE__, __FUNCTION__, __VA_ARGS__)
#define log_info(...)  Log::instance().log_api( LOG_INFO, __FILE__, __LINE__, __FUNCTION__, __VA_ARGS__)
#define log_debug(...)  Log::instance().log_api( LOG_DEBUG, __FILE__, __LINE__, __FUNCTION__, __VA_ARGS__)

#else  // TZMONITOR_CLIENT, BOOST_TEST_MODULE

static void log_api(const char* priority, const char *file, int line, const char *func, const char *msg, ...) {

    char buf[MAX_LOG_BUF_SIZE + 2] = {0, };
    int n = snprintf(buf, MAX_LOG_BUF_SIZE, "[%s:%d][%s][%#lx]<%s> -- ", file, line, func, (long)pthread_self(), priority);

    va_list arg_ptr;
    va_start(arg_ptr, msg);
    vsnprintf(buf + n, MAX_LOG_BUF_SIZE - n, msg, arg_ptr);
    va_end(arg_ptr);

    n = static_cast<int>(strlen(buf));
    if (std::find(buf, buf + n, '\n') == (buf + n)) {
        buf[n] = '\n';   // 兼容老的pbi_log_service
        fprintf(stderr, "%s", buf);
        return;
    }
}

#define log_emerg(...)   log_api( "EMERG", __FILE__, __LINE__, __FUNCTION__, __VA_ARGS__)
#define log_alert(...)   log_api( "ALERT", __FILE__, __LINE__, __FUNCTION__, __VA_ARGS__)
#define log_crit(...)    log_api( "CRIT", __FILE__, __LINE__, __FUNCTION__, __VA_ARGS__)
#define log_err(...)     log_api( "ERR", __FILE__, __LINE__, __FUNCTION__, __VA_ARGS__)
#define log_warning(...) log_api( "WARNING", __FILE__, __LINE__, __FUNCTION__, __VA_ARGS__)
#define log_notice(...)  log_api( "NOTICE", __FILE__, __LINE__, __FUNCTION__, __VA_ARGS__)
#define log_info(...)    log_api( "INFO", __FILE__, __LINE__, __FUNCTION__, __VA_ARGS__)
#define log_debug(...)   log_api( "DEBUG", __FILE__, __LINE__, __FUNCTION__, __VA_ARGS__)


#endif // TZMONITOR_CLIENT, BOOST_TEST_MODULE


#endif // _TZ_LOG_H_
