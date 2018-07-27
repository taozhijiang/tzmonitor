#include <signal.h>
void backtrace_init();

#include <syslog.h>
#include <boost/format.hpp>
#include <boost/atomic/atomic.hpp>

#include "config.h"

#include "General.h"
#include "Manager.h"
#include "Helper.h"

#include <utils/Utils.h>
#include <utils/Log.h>
#include <utils/SslSetup.h>


struct tm service_start{};
std::string TZ_VERSION;

static void init_signal_handle(){

    ::signal(SIGPIPE, SIG_IGN);

    ::signal(SIGUSR1, SIG_IGN);
    ::signal(SIGUSR2, SIG_IGN);

    return;
}

static void show_vcs_info () {


    std::cout << " THIS RELEASE OF TZMonitor " << std::endl;

    TZ_VERSION = boost::str( boost::format("v%d.%d.%d") %TZ_VERSION_MAJOR %TZ_VERSION_MINOR %TZ_VERSION_PATCH);
    std::cout << "      VERSION: "  << TZ_VERSION << std::endl;

    extern const char *build_commit_version;
    extern const char *build_commit_branch;
    extern const char *build_commit_date;
    extern const char *build_commit_author;
    extern const char *build_time;

    std::cout << build_commit_version << std::endl;
    std::cout << build_commit_branch << std::endl;
    std::cout << build_commit_date << std::endl;
    std::cout << build_commit_author << std::endl;
    std::cout << build_time << std::endl;

    return;
}


// /var/run/[program_invocation_short_name].pid --> root permission
static int create_process_pid() {

    char pid_msg[24];
    char pid_file[PATH_MAX];

    snprintf(pid_file, PATH_MAX, "./%s.pid", program_invocation_short_name);
    FILE* fp = fopen(pid_file, "w+");

    if (!fp) {
        log_err("Create pid file %s failed!", pid_file);
        return -1;
    }

    pid_t pid = ::getpid();
    snprintf(pid_msg, sizeof(pid_msg), "%d\n", pid);
    fwrite(pid_msg, sizeof(char), strlen(pid_msg), fp);

    fclose(fp);
    return 0;
}

int main(int argc, char* argv[]) {

    show_vcs_info();

    std::string config_file = "tzmonitor.conf";
    if (!sys_config_init(config_file)) {
        std::cout << "Handle system configure failed!" << std::endl;
        return -1;
    }

    int log_level = 0;
    if (!get_config_value("log_level", log_level)) {
        log_level = LOG_INFO;
        log_info("Using default log_level LOG_INFO");
    }

    set_checkpoint_log_store_func(syslog);
    if (!log_init(log_level)) {
        std::cerr << "Init syslog failed!" << std::endl;
        return -1;
    }

    log_debug("syslog initialized ok!");
    time_t now = ::time(NULL);
    ::localtime_r(&now, &service_start);
    log_info("Service start at %s", ::asctime(&service_start));

    // test boost::atomic
    boost::atomic<int> atomic_int;
    if (atomic_int.is_lock_free()) {
        log_alert("GOOD, your system atomic is lock_free ...");
    } else {
        log_err("BAD, your system atomic is not lock_free, may impact performance ...");
    }

    // SSL 环境设置
    if (!Ssl_thread_setup()) {
        log_err("SSL env setup error!");
        ::exit(1);
    }


    (void)Manager::instance(); // create object first!

    create_process_pid();
    init_signal_handle();
    backtrace_init();

    {
        PUT_COUNT_FUNC_PERF(Manager_init);
        if(!Manager::instance().init()) {
            log_err("Manager init error!");
            ::exit(1);
        }
    }

    log_debug( "TZMonitor service initialized ok!");
    Manager::instance().service_joinall();

    Ssl_thread_clean();

    return 0;
}
