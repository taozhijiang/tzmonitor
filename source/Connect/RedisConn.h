/*-
 * Copyright (c) 2019 TAO Zhijiang<taozhijiang@gmail.com>
 *
 * Licensed under the BSD-3-Clause license, see LICENSE for full information.
 *
 */


#ifndef __CONNECT_REDIS_CONN_H__
#define __CONNECT_REDIS_CONN_H__


#include <hiredis/hiredis.h>
#include <hiredis/async.h>

#include <boost/optional.hpp>


#include <Connect/ConnPool.h>

namespace tzrpc {


class RedisConn;
typedef std::shared_ptr<RedisConn> redis_conn_ptr;

struct RedisConnPoolHelper {
public:
    RedisConnPoolHelper(std::string host, int port,
                        std::string passwd, int db_idx = 0):
        host_(host), port_(port),
        passwd_(passwd), db_idx_(db_idx) {
    }

public:
    const std::string host_;
    const int port_;
    const std::string passwd_;
    const int db_idx_;
};

typedef std::shared_ptr<redisReply> redisReply_ptr;

class RedisConn {
public:
    explicit RedisConn(ConnPool<RedisConn, RedisConnPoolHelper>& pool, const RedisConnPoolHelper& helper):
        pool_(pool), helper_(helper) {
    }

    virtual ~RedisConn(){
        log_info("Destroy Sql Connection OK!");
    }

    // 禁止拷贝
    RedisConn(const RedisConn&) = delete;
    RedisConn& operator=(const RedisConn&) = delete;

    bool init(int64_t conn_uuid);
    bool is_health() {
        return isValid();
    }

    int LoadScript(const std::string& script, std::string& sha);
    int CheckSha(const std::string& sha, bool& sha_exist);

    redisReply_ptr Exec(const char *format, ...);
    redisReply_ptr Exec(int argc, const char **argv);

    redisReply_ptr ExecV(std::vector<const char*>& argv);
    redisReply_ptr ExecV(const std::vector<std::string>& argv);

    redisReply_ptr Eval(const std::string& script, const std::vector<std::string>& keys,
                        const std::vector<std::string>& args);

    operator bool();

public:

    // more elegent api
   /**
     * Redis command wrappers.
     */

    // SELECT  OK -> bool
    bool Select(int db_idx);

    // EXISTS  low-level 1 exists, 0 not
    bool Exists(const std::string& key);

    // GET
    redisReply_ptr Get(const std::string& key);
    bool Get(const std::string& key, int64_t& value);
    bool Get(const std::string& key, std::string& value);

    // DEL  返回被删除 key 的数量
    int Del(const std::string& key);
    int Del(const std::vector<std::string>& keys);

    // SET
    bool Set(const std::string& key, int64_t value, int expire = -1);
    bool Set(const std::string& key, const std::string& value, int expire = -1);

    // MGET
    redisReply_ptr MGet(const std::vector<std::string>& keys);
    bool MGet(const std::vector<std::string>& keys, std::vector<int64_t>& values, int64_t nil_value = 0);
    bool MGet(const std::vector<std::string>& keys, std::vector<std::string>& values, const std::string& nil_value = "");

    // MSET   总是返回 OK
    bool MSet(const std::vector<std::string>& keys, const std::vector<int64_t>& values);
    bool MSet(const std::vector<std::string>& keys, const std::vector<std::string>& values);

    // INCR
    boost::optional<int64_t> Incr(const std::string& key);
    boost::optional<int64_t> IncrBy(const std::string& key, int64_t value);
    boost::optional<int64_t> HIncrBy(const std::string& key, const std::string& hkey, int64_t value);

    // DECR
    boost::optional<int64_t> Decr(const std::string& key);
    boost::optional<int64_t> DecrBy(const std::string& key, int64_t value);

    // Expire 设置成功返回 1
    int Expire(const std::string& key, int expire);
    int MExpire(const std::vector<std::string>& keys, int expire);

    // HSet
    int HSet(const std::string& key, const std::string& hkey, const int32_t value);
    int HSet(const std::string& key, const std::string& hkey, const int64_t value);
    int HSet(const std::string& key, const std::string& hkey, const std::string& value);

    // HGet
    redisReply_ptr HGet(const std::string& key, const std::string& hkey);
    bool HGet(const std::string& key, const std::string& hkey, int64_t& value);
    bool HGet(const std::string& key, const std::string& hkey, std::string& value);

    // HGetAll
    redisReply_ptr HGetAll(const std::string& key);
    bool HGetAll(const std::string& key, std::vector<std::string>& values);
    bool HGetAll(const std::string& key, std::map<std::string, int64_t>& values);
    bool HGetAll(const std::string& key, std::map<std::string, std::string>& values);

    // List
    // 执行 LPUSH 命令后，列表的长度
    int LPush(const std::string& key, const std::string& value);
    int LPush(const std::string& key, const std::vector<std::string>& values);
    bool RPop(const std::string& key, std::string& value);

    int LPush(const std::string& key, int64_t value);
    bool RPop(const std::string& key, int64_t& value);

private:
    int listPush(const char* cmd, const std::string& key, const std::string& value);
    int listPush(const char* cmd, const std::string& key, const std::vector<std::string>& values);
    bool listPop(const char* cmd, const std::string& key, std::string& value);

    int listPush(const char* cmd, const std::string& key, int64_t value);
    bool listPop(const char* cmd, const std::string& key, int64_t& value);


private:

    bool isValid() {
        return (context_ && context_->err == 0);
    }

    bool CHECK_N_RECONNECT() {
        if (isValid()) {
            return true;
        }

        // TODO ...

        return isValid();
    }


    bool replyOK(const redisReply_ptr& reply) {

        if(!reply ||
           reply->type != REDIS_REPLY_STATUS ||
           reply->str == NULL ||
           ::strcmp(reply->str, "OK") != 0) {
            return false;
        }

        return true;
    }
    std::shared_ptr<redisContext> context_;

    // may be used in future
    ConnPool<RedisConn, RedisConnPoolHelper>& pool_;
    const RedisConnPoolHelper helper_;
};


} // end namespace tzrpc

#endif // __CONNECT_REDIS_CONN_H__

