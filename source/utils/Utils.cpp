/*-
 * Copyright (c) 2018 TAO Zhijiang<taozhijiang@gmail.com>
 *
 * Licensed under the BSD-3-Clause license, see LICENSE for full information.
 *
 */


#include <execinfo.h>

#include <sys/types.h>
#include <signal.h>

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

void COUNT_FUNC_PERF::display_info(const std::string& env, int64_t time_ms, int64_t time_us) {
    log_debug("%s, %s perf: %ld.%ld ms", env_.c_str(), key_.c_str(), time_ms, time_us);
}
