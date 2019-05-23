/*-
 * Copyright (c) 2019 TAO Zhijiang<taozhijiang@gmail.com>
 *
 * Licensed under the BSD-3-Clause license, see LICENSE for full information.
 *
 */

#include <string>
#include <iostream>
#include <syslog.h>

#include <tzhttpd/CheckPoint.h>
#include <tzhttpd/Log.h>
#include <tzhttpd/HttpServer.h>

#include <Client/include/HeraclesClient.h>

#include "stat_handler.h"

extern char * program_invocation_short_name;
static void usage() {
    std::stringstream ss;

    ss << program_invocation_short_name << ":" << std::endl;
    ss << "\t -c cfgFile  specify config file, default heracles.conf. " << std::endl;
    ss << "\t -d          daemonize service." << std::endl;
    ss << "\t -v          print version info." << std::endl;
    ss << std::endl;

    std::cout << ss.str();
}

char cfgFile[PATH_MAX] = "heracles.conf";
bool daemonize = false;


static void interrupted_callback(int signal){
    tzhttpd::tzhttpd_log_alert("Signal %d received ...", signal);
    switch(signal) {
        case SIGHUP:
            tzhttpd::tzhttpd_log_notice("SIGHUP recv, do update_run_conf... ");
            tzhttpd::ConfHelper::instance().update_runtime_conf();
            break;

        case SIGUSR1:
            tzhttpd::tzhttpd_log_notice("SIGUSR recv, do module_status ... ");
            {
                std::string output;
                tzhttpd::Status::instance().collect_status(output);
                std::cout << output << std::endl;
                tzhttpd::tzhttpd_log_notice("%s", output.c_str());
            }
            break;

        default:
            tzhttpd::tzhttpd_log_err("Unhandled signal: %d", signal);
            break;
    }
}

static void init_signal_handle(){

    ::signal(SIGPIPE, SIG_IGN);
    ::signal(SIGUSR1, interrupted_callback);
    ::signal(SIGHUP,  interrupted_callback);

    return;
}

static int module_status(std::string& strModule, std::string& strKey, std::string& strValue) {

    strModule = "http_face";
    strKey = "main";

    strValue = "conf_file: " + std::string(cfgFile);
    return 0;
}

int main(int argc, char* argv[]) {

    int opt_g = 0;
    while( (opt_g = getopt(argc, argv, "c:dhv")) != -1 ) {
        switch(opt_g)
        {
            case 'c':
                memset(cfgFile, 0, sizeof(cfgFile));
                strncpy(cfgFile, optarg, PATH_MAX);
                break;
            case 'd':
                daemonize = true;
                break;
            case 'v':
                std::cout << program_invocation_short_name << ": "
                    << tzhttpd::http_handler::http_server_version << std::endl;
                break;
            case 'h':
            default:
                usage();
                ::exit(EXIT_SUCCESS);
        }
    }


    libconfig::Config cfg;
    std::shared_ptr<tzhttpd::HttpServer> http_server_ptr;

    // default syslog
    tzhttpd::set_checkpoint_log_store_func(syslog);
    // setup in default DEBUG level, then reinialize when conf prased
    tzhttpd::tzhttpd_log_init(7);
    tzhttpd::tzhttpd_log_debug("first stage log init with default DEBUG finished.");

    // daemonize should before any thread creation...
    if (daemonize) {
        tzhttpd::tzhttpd_log_notice("we will daemonize this service...");

        bool chdir = false; // leave the current working directory in case
                            // the user has specified relative paths for
                            // the config file, etc
        bool close = true;  // close stdin, stdout, stderr
        if (::daemon(!chdir, !close) != 0) {
            tzhttpd::tzhttpd_log_err("call to daemon() failed: %s", strerror(errno));
            ::exit(EXIT_FAILURE);
        }
    }

    // 信号处理
    init_signal_handle();

    http_server_ptr.reset(new tzhttpd::HttpServer(cfgFile, "Heracles"));
    if (!http_server_ptr ) {
        tzhttpd::tzhttpd_log_err("create HttpServer failed!");
        ::exit(EXIT_FAILURE);
    }

    if(!http_server_ptr->init()){
        tzhttpd::tzhttpd_log_err("init HttpServer failed!");
        ::exit(EXIT_FAILURE);
    }

    // test monitor first stage
    auto reporter = std::make_shared<heracles_client::HeraclesClient>();
    if (!reporter || !reporter ->init(cfgFile)) {
        tzhttpd::tzhttpd_log_err("init client failed.");
        return false;
    }

    if (reporter->ping()) {
        tzhttpd::tzhttpd_log_err("client call ping failed.");
        return false;
    }

    http_server_ptr->add_http_get_handler("^/monitor$", tzhttpd::http_handler::index_http_get_handler);
    http_server_ptr->add_http_get_handler("^/monitor/stats$", tzhttpd::http_handler::stats_http_get_handler);

    http_server_ptr->register_http_runtime_callback(
            "http_face",
            std::bind(&heracles_client::HeraclesClient::module_runtime, reporter,
                      std::placeholders::_1));

    http_server_ptr->register_http_status_callback(
            "http_face",
            std::bind(&heracles_client::HeraclesClient::module_status, reporter,
                      std::placeholders::_1, std::placeholders::_2,
                      std::placeholders::_3));

    http_server_ptr->register_http_status_callback("httpsrv", module_status);

    http_server_ptr->io_service_threads_.start_threads();
    http_server_ptr->service();

    http_server_ptr->io_service_threads_.join_threads();

    return 0;
}

namespace boost {

void assertion_failed(char const * expr, char const * function, char const * file, long line) {
    fprintf(stderr, "BAD!!! expr `%s` assert failed at %s(%ld): %s", expr, file, line, function);
    tzhttpd::tzhttpd_log_err("BAD!!! expr `%s` assert failed at %s(%ld): %s", expr, file, line, function);
}

} // end boost

