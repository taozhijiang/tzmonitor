/*-
 * Copyright (c) 2019 TAO Zhijiang<taozhijiang@gmail.com>
 *
 * Licensed under the BSD-3-Clause license, see LICENSE for full information.
 *
 */

#ifndef __SCAFFOLD_CAPTAIN_H__
#define __SCAFFOLD_CAPTAIN_H__

#include <memory>
#include <string>
#include <map>
#include <vector>

namespace roo {
class Setting;
class Status;
class Timer;
}

namespace tzrpc {
class NetServer;
}

class Captain {
public:
    static Captain& instance();

public:
    bool init(const std::string& cfgFile);

    bool service_joinall();
    bool service_graceful();
    void service_terminate();

private:
    Captain();

    ~Captain() {
        // Singleton should not destoried normally,
        // if happens, just terminate quickly
        ::exit(EXIT_SUCCESS);
    }


    bool initialized_;

public:

    // 网络主框架
    std::shared_ptr<tzrpc::NetServer> net_server_ptr_;

    std::shared_ptr<roo::Setting> setting_ptr_;
    std::shared_ptr<roo::Status> status_ptr_;
    std::shared_ptr<roo::Timer> timer_ptr_;

};



#endif //__SCAFFOLD_CAPTAIN_H__