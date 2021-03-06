/*-
 * Copyright (c) 2019 TAO Zhijiang<taozhijiang@gmail.com>
 *
 * Licensed under the BSD-3-Clause license, see LICENSE for full information.
 *
 */


#include <libconfig.h++>

#include <Utils/Log.h>

#include <Scaffold/ConfHelper.h>

#include <Business/StoreIf.h>
#include <Business/StoreSql.h>
#include <Business/StoreLevelDB.h>

using namespace tzrpc;

std::mutex init_lock_;

std::shared_ptr<StoreIf> StoreFactory(const std::string& storeType) {

    static std::shared_ptr<StoreIf> mysql_ {};
    static std::shared_ptr<StoreIf> redis_ {};
    static std::shared_ptr<StoreIf> leveldb_ {};

    static std::shared_ptr<StoreIf> NULLPTR {};

    if (storeType == "mysql") {
        if (mysql_) {
            return mysql_;
        }

        std::lock_guard<std::mutex> lock(init_lock_);
        if (mysql_) {
            return mysql_;
        }

        // 初始化
        auto conf_ptr = ConfHelper::instance().get_conf();
        if (!conf_ptr) {
            log_err("ConfHelper not initialized, please check your initialize order.");
            return NULLPTR;
        }
        std::shared_ptr<StoreIf> mysql = std::make_shared<StoreSql>();
        if (mysql && mysql->init(*conf_ptr)) {
            log_debug("create and initialized StoreSql OK!");
            mysql_.swap(mysql);
            return mysql_;
        }

        return NULLPTR;

    } else if (storeType == "redis") {

        log_err("redis store not implemented yet!");
        return NULLPTR;

    } else if (storeType == "leveldb") {

        if (leveldb_) {
            return leveldb_;
        }

        std::lock_guard<std::mutex> lock(init_lock_);
        if (leveldb_) {
            return leveldb_;
        }

        // 初始化
        auto conf_ptr = ConfHelper::instance().get_conf();
        if (!conf_ptr) {
            log_err("ConfHelper not initialized, please check your initialize order.");
            return NULLPTR;
        }
        std::shared_ptr<StoreIf> leveldb = std::make_shared<StoreLevelDB>();
        if (leveldb && leveldb->init(*conf_ptr)) {
            log_debug("create and initialized StoreLevelDB OK!");
            leveldb_.swap(leveldb);
            return leveldb_;
        }

        return NULLPTR;

    } else {
        log_err("Invalid storeType: %s", storeType.c_str());
        return NULLPTR;
    }
}



