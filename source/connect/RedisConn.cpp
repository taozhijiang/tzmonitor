#include "General.h"

#include <boost/lexical_cast.hpp>

#include <utils/Log.h>

#include "RedisConn.h"
#define INVALID_INT (-9223372036854775807L-1L)


// #define REDIS_REPLY_ERROR 0
// #define REDIS_REPLY_STRING 1
// #define REDIS_REPLY_ARRAY 2
// #define REDIS_REPLY_INTEGER 3
// #define REDIS_REPLY_NIL 4

template <typename T>
static std::string local_convert_to_string(const T& arg) {
    try {
        return boost::lexical_cast<std::string>(arg);
    }
    catch(boost::bad_lexical_cast& e) {
        return "";
    }
}

static void printReply(const redisReply* reply) {

    if(!reply) {
        std::cout << "REPLY NULL" << std::endl;
        return;
    }

    std::cout << "reply: " << reply->type << " ";
    switch(reply->type) {
        case REDIS_REPLY_STRING:
        case REDIS_REPLY_STATUS:
        case REDIS_REPLY_ERROR:
            if(reply->str == NULL) {
                std::cout << "<NULL>";
            } else {
                std::cout << reply->str;
            }
            break;
        case REDIS_REPLY_ARRAY:
            std::cout << std::endl;
            for(size_t i = 0; i < reply->elements; i ++) {
                printReply(reply->element[i]);
            }
            break;
        case REDIS_REPLY_INTEGER:
            std::cout << reply->integer;
            break;
        case REDIS_REPLY_NIL:
            std::cout << "<nil>";
            break;
        default:
            break;
    }
}


bool RedisConn::init(int64_t conn_uuid) {

    set_uuid(conn_uuid);

    struct timeval tv;
    tv.tv_sec = 5;
    tv.tv_usec = 0;
    context_.reset(redisConnectWithTimeout(helper_.host_.c_str(), helper_.port_, tv),  redisFree);
    if (!context_ || context_->err) {
        if (context_->err) {
            log_err("redis context_ problem: %s", context_->errstr);
            context_.reset();
        } else {
            log_err("create context error");
        }
        return false;
    }

    if (helper_.passwd_.empty()) // same as == ""
        return true;

    redisReply_ptr r( (redisReply *)redisCommand(context_.get(), "AUTH %s", helper_.passwd_.c_str()), freeReplyObject);
    if (!r) {
        log_err("redis conn auth error!");
        context_.reset();
        return false;
    }

    if (r->type == REDIS_REPLY_ERROR) {
        log_err("redis conn auth failed!");
        context_.reset();
        return false;
    }

    if (helper_.db_idx_ != 0) {
        if(!Select(helper_.db_idx_)) {
            log_err("redis select db failed: %d", helper_.db_idx_);
            context_.reset();
            return false;
        }
    }

    return true;
}

// 尽可能过滤通用的错误情况
// 错误情况下reply直接返回空信息
redisReply_ptr RedisConn::Exec(const char *fmt, ...) {

    redisReply_ptr reply {};
    if(!CHECK_N_RECONNECT())
        return reply;

    va_list ap;
    va_start(ap, fmt);
    reply.reset( (redisReply *)redisvCommand(context_.get(), fmt, ap), freeReplyObject);
    va_end(ap);

    if (!reply) {
        if (context_ && context_->err) {
            log_err("exec failed supply context_ info: %s", context_->errstr);
        } else {
            log_err("exec failed with empty reply.");
        }
        return reply;
    }

    if(reply && reply->type == REDIS_REPLY_ERROR ) {
        if(reply->str) {
            log_err("exec failed with error: %s", reply->str);
        } else {
            log_err("exec failed");
        }
        reply.reset();
    }

    return reply;
}

redisReply_ptr RedisConn::Exec(int argc, const char **argv) {

    redisReply_ptr reply {};
    if(!CHECK_N_RECONNECT())
        return reply;

    reply.reset( (redisReply *)redisCommandArgv(context_.get(), argc ,argv, 0 ), freeReplyObject);

    if (!reply) {
        if (context_ && context_->err) {
            log_err("exec failed supply context_ info: %s", context_->errstr);
        } else {
            log_err("exec failed with empty reply.");
        }
        return reply;
    }

    if(reply && reply->type == REDIS_REPLY_ERROR ) {
        if(reply->str) {
            log_err("exec failed with error: %s", reply->str);
        } else {
            log_err("exec failed");
        }
        reply.reset();
    }

    return reply;
}

redisReply_ptr RedisConn::ExecV(std::vector<const char*>& argv) {

    redisReply_ptr reply {};
    if(!CHECK_N_RECONNECT())
        return reply;

    reply.reset( (redisReply *)redisCommandArgv(context_.get(), static_cast<int>(argv.size()), &argv[0], NULL), freeReplyObject);
    if (!reply) {
        if (context_ && context_->err) {
            log_err("execv failed supply context_ info: %s", context_->errstr);
        } else {
            log_err("execv failed with empty reply.");
        }
        return reply;
    }

    if(reply && reply->type == REDIS_REPLY_ERROR ) {
        if(reply->str) {
            log_err("execv failed with error: %s", reply->str);
        } else {
            log_err("execv failed");
        }
        reply.reset();
    }

    return reply;
}

redisReply_ptr RedisConn::ExecV(const std::vector<std::string>& argv) {

    redisReply_ptr reply {};
    if(!CHECK_N_RECONNECT())
        return reply;

    std::vector<const char*> argvptrs;
    for(std::vector<std::string>::const_iterator it = argv.begin(); it != argv.end(); it ++) {
        argvptrs.push_back((*it).c_str());
    }
    reply = ExecV(argvptrs);

    if (!reply) {
        if (context_ && context_->err) {
            log_err("execv failed supply context_ info: %s", context_->errstr);
        } else {
            log_err("execv failed with empty reply.");
        }
        return reply;
    }

    if(reply && reply->type == REDIS_REPLY_ERROR ) {
        if(reply->str) {
            log_err("execv failed with error: %s", reply->str);
        } else {
            log_err("execv failed");
        }
        reply.reset();
    }

    return reply;
}

redisReply_ptr RedisConn::Eval(const std::string& script,
                               const std::vector<std::string>& keys,
                               const std::vector<std::string>& args) {

    redisReply_ptr reply {};

    if(script.empty()) {
        return reply; // empty
    }

    size_t arg_num = 1 + 1 + 1 + keys.size() + args.size();  // cmd + script + keys num + keys + args
    std::vector<const char*> argvs;

    argvs.reserve(arg_num);
    const char *cmd = "EVAL";
    argvs.push_back(cmd);
    argvs.push_back(script.c_str());
    std::string str_keysnum = local_convert_to_string(keys.size());
    argvs.push_back(str_keysnum.c_str());

    size_t index = 0;
    for(index = 0; index < keys.size(); index ++) {  // The keys
        argvs.push_back(keys[index].c_str());
    }

    for(index = 0; index < args.size(); index ++) {
        argvs.push_back(args[index].c_str());
    }

    reply.reset( (redisReply*)redisCommandArgv(context_.get(), static_cast<int>(argvs.size()), &argvs[0], NULL), freeReplyObject);
    if (!reply) {
        if (context_ && context_->err) {
            log_err("eval failed supply context_ info: %s", context_->errstr);
        } else {
            log_err("eval failed with empty reply.");
        }
        return reply;
    }

    if(reply && reply->type == REDIS_REPLY_ERROR ) {
        if(reply->str) {
            log_err("eval failed with error: %s", reply->str);
        } else {
            log_err("eval failed");
        }
        reply.reset();
    }

    return reply;
}


RedisConn::operator bool() {
    return !!context_;
}


int RedisConn::LoadScript(const std::string& script, std::string& sha) {

    int nResult = 0;

    do {
        log_debug("SCRIPT LOAD %s ", script.c_str());
        redisReply_ptr r = Exec("SCRIPT LOAD %s ", script.c_str());

        if (!r) {
            log_err("redis script load error!");
            nResult = -2;
            break;
        }

        if (r->type != REDIS_REPLY_STRING || !r->len) {
            log_err("expecting return eval_sha error");
            nResult = -3;
            break;
        }

        sha = std::string( r->str, r->len );
        log_alert("Success load with new sha: %s", sha.c_str());

    } while (0);

    return nResult;
}


int RedisConn::CheckSha(const std::string& sha, bool& sha_exist) {

    int nResult = 0;

    do {
        log_debug("SCRIPT EXISTS %s", sha.c_str());
        redisReply_ptr r = Exec("SCRIPT EXISTS %s", sha.c_str());

        if (!r ) {
            nResult = -1;
            log_err("get redis resp error!");
            break;
        }

        if (r->type == REDIS_REPLY_ARRAY && r->elements >0 && r->element[0]->integer == 1) {
            sha_exist = true;
            log_alert("script %s exist!", sha.c_str());
        } else if (r->type == REDIS_REPLY_ARRAY && r->elements >0 && r->element[0]->integer == 0) {
            sha_exist = false;
            log_err("script %s not exist!", sha.c_str());
        } else {
            nResult = -2;
            log_err("Unknow response: %d", r->type);
        }

    } while (0);

    return nResult;
}


// API

bool RedisConn::Select(int db_idx) {

    redisReply_ptr reply = Exec("SELECT %d", db_idx);
    return replyOK(reply);
}

bool RedisConn::Exists(const std::string& key) {

    redisReply_ptr reply = Exec("EXISTS %s", key.c_str());
    if(reply && reply->type == REDIS_REPLY_INTEGER) {
        return reply->integer == 1;
    }

    return false;
}


redisReply_ptr RedisConn::Get(const std::string& key) {

    redisReply_ptr reply = Exec("GET %s", key.c_str());
    if(!reply) {
        log_err("GET %s failed", key.c_str());
        return reply;
    }

    if(reply->type == REDIS_REPLY_NIL) {
        log_err("GET %s nil", key.c_str());
        reply.reset();
    }

    return reply;
}

bool RedisConn::Get(const std::string& key, int64_t& value) {

    redisReply_ptr reply = Get(key);
    if(!reply)
        return false;

    if(reply->type == REDIS_REPLY_INTEGER) {
        value = reply->integer;  // maybe ?
        return true;
    } else if(reply->type == REDIS_REPLY_STRING && reply->str != NULL) {
        value = ::atoll(reply->str);
        return true;
    } else if(reply->type == REDIS_REPLY_NIL) {
        // empty
        return false;
    }

    log_err("unknown reply->type: %d", reply->type);
    return false;
}

bool RedisConn::Get(const std::string& key, std::string& value) {

    redisReply_ptr reply = Get(key);
    if(!reply)
        return false;

    if(reply->type == REDIS_REPLY_INTEGER) {
        value = local_convert_to_string(reply->integer);
        return true;
    } else if(reply->type == REDIS_REPLY_STRING && reply->str != NULL) {
        value = reply->str;
        return true;
    } else if(reply->type == REDIS_REPLY_NIL) {
        // empty
        return false;
    }

    log_err("unknown reply->type: %d", reply->type);
    return false;
}

int RedisConn::Del(const std::string& key) {

    redisReply_ptr reply = Exec("DEL %s", key.c_str());
    if (!reply) {
        return -1;
    }

    if (reply && reply->type == REDIS_REPLY_INTEGER) {
        return reply->integer;
    }

    log_err("unknown return->type: %d", reply->type);
    return -1;
}

int RedisConn::Del(const std::vector<std::string>& keys) {

    const char* cmd = "DEL";
    std::vector<const char*> argv;
    argv.reserve(keys.size() + 1);
    argv.push_back(cmd);
    for(size_t index = 0; index < keys.size(); index ++) {
        argv.push_back(keys[index].c_str());
    }

    redisReply_ptr reply = ExecV(argv);
    if (!reply) {
        return -1;
    }

    if(reply && reply->type == REDIS_REPLY_INTEGER) {
        return reply->integer;
    }

    log_err("unknown return->type: %d", reply->type);
    return -1;
}

bool RedisConn::Set(const std::string& key, const std::string& value, int expire) {

    redisReply_ptr reply {};
    if(expire > 0) {
        reply = Exec("SET %s %s EX %d", key.c_str(), value.c_str(), expire);
    } else {
        reply = Exec("SET %s %s", key.c_str(), value.c_str());
    }

    return replyOK(reply);
}

bool RedisConn::Set(const std::string& key, int64_t value, int expire) {

    redisReply_ptr reply {};
    if(expire > 0) {
        reply = Exec("SET %s %ld EX %d", key.c_str(), value, expire);
    } else {
        reply = Exec("SET %s %ld", key.c_str(), value, expire);
    }

    return replyOK(reply);
}


redisReply_ptr RedisConn::MGet(const std::vector<std::string>& keys) {

    redisReply_ptr reply {};
    if(keys.empty()) {
        log_err("provided keys empty");
        return reply;
    }

    const char* cmd = "MGET";
    std::vector<const char*> argv;
    argv.reserve(keys.size() + 1);
    argv.push_back(cmd);
    for(size_t index = 0; index < keys.size(); index ++) {
        argv.push_back(keys[index].c_str());
    }

    reply = ExecV(argv);
    if (!reply || reply->type == REDIS_REPLY_NIL) {
        reply.reset();
        return reply;
    }

    if (reply->type == REDIS_REPLY_ARRAY) {
        return reply;
    }

    log_err("MGET unexpected reply->type: %d", reply->type);
    reply.reset();
    return reply;
}

bool RedisConn::MGet(const std::vector<std::string>& keys, std::vector<int64_t>& values,
                     int64_t nil_value) {

    redisReply_ptr reply = MGet(keys);
    if(!reply)
        return false;

    for(size_t index = 0; index < reply->elements; index ++) {
        if(reply->element[index]->type == REDIS_REPLY_INTEGER) {
            values.push_back(reply->element[index]->integer);
        } else if(reply->element[index]->type == REDIS_REPLY_STRING && reply->element[index]->str != NULL) {
            values.push_back(::atoll(reply->element[index]->str));
        } else {
            values.push_back(nil_value);
        }
    }

    return true;
}

bool RedisConn::MGet(const std::vector<std::string>& keys, std::vector<std::string>& values,
                     const std::string& nil_value) {

    redisReply_ptr reply = MGet(keys);
    if(!reply)
        return false;

    for(size_t index = 0; index < reply->elements; index ++) {
        if(reply->element[index]->type == REDIS_REPLY_INTEGER) {
            values.push_back(local_convert_to_string(reply->element[index]->integer));
        } else if(reply->element[index]->type == REDIS_REPLY_STRING && reply->element[index]->str != NULL) {
            values.push_back(reply->element[index]->str);
        } else {
            values.push_back(nil_value);
        }
    }

    return true;
}

bool RedisConn::MSet(const std::vector<std::string>& keys, const std::vector<int64_t>& values) {

    if(keys.empty() || keys.size() != values.size()) {
        log_err("param error");
        return false;
    }

    std::vector<std::string> str_values;
    str_values.reserve(values.size());
    for(size_t index = 0; index < keys.size(); index ++) {
        str_values.push_back(local_convert_to_string(values[index]));
    }
    return MSet(keys, str_values);
}

bool RedisConn::MSet(const std::vector<std::string>& keys, const std::vector<std::string>& values) {

    if(keys.empty() || keys.size() != values.size()) {
        log_err("param error");
        return false;
    }

    const char* cmd = "MSET";
    std::vector<const char*> argv;
    argv.reserve(keys.size() * 2 + 1);
    argv.push_back(cmd);
    for(size_t index = 0; index < keys.size(); index ++) {
        argv.push_back(keys[index].c_str());
        argv.push_back(values[index].c_str());
    }

    redisReply_ptr reply = ExecV(argv);
    return replyOK(reply);
}


boost::optional<int64_t> RedisConn::Incr(const std::string& key) {

    redisReply_ptr reply = Exec("INCR %s", key.c_str());
    if(reply && reply->type == REDIS_REPLY_INTEGER) {
        return reply->integer;
    }

    return boost::none;
}

boost::optional<int64_t> RedisConn::IncrBy(const std::string& key, int64_t value) {

    redisReply_ptr reply = Exec("INCRBY %s %ld", key.c_str(), value);
    if(reply && reply->type == REDIS_REPLY_INTEGER) {
        return reply->integer;
    }

    return boost::none;
}

boost::optional<int64_t> RedisConn::HIncrBy(const std::string& key, const std::string& hkey, int64_t value) {

    redisReply_ptr reply = Exec("HINCRBY %s %s %ld", key.c_str(), hkey.c_str(), value);
    if(reply && reply->type == REDIS_REPLY_INTEGER) {
        return reply->integer;
    }

    return boost::none;
}

boost::optional<int64_t> RedisConn::Decr(const std::string& key) {

    redisReply_ptr reply = Exec("DECR %s", key.c_str());
    if(reply && reply->type == REDIS_REPLY_INTEGER) {
        return reply->integer;
    }

    return boost::none;
}

boost::optional<int64_t> RedisConn::DecrBy(const std::string& key, int64_t value) {

    redisReply_ptr reply = Exec("DECRBY %s %ld", key.c_str(), value);
    if(reply && reply->type == REDIS_REPLY_INTEGER) {
        return reply->integer;
    }

    return boost::none;
}


// 设置成功返回 1
int RedisConn::Expire(const std::string& key, int expire) {

    if(key.empty() || expire < 0) {
        return -1;
    }

    redisReply_ptr reply = Exec("EXPIRE %s %d", key.c_str(), expire);
    if(reply && reply->type == REDIS_REPLY_INTEGER) {
        return reply->integer;
    }

    return -2;
}

int RedisConn::MExpire(const std::vector<std::string>& keys, int expire) {

    if(expire < 0 || keys.size() == 0) {
        return -1;
    }

    std::string script = "for k, v in ipairs(KEYS) do redis.pcall('EXPIRE', v, ARGV[1]); end; return 0;";
    std::vector<std::string> args;
    args.push_back(local_convert_to_string(expire));
    redisReply_ptr reply = Eval(script, keys, args);

    return 0;
}


// HSet
// 新建字段并且值设置成功，返回 1；如果哈希表中域字段已经存在且旧值已被新值覆盖，返回 0
int RedisConn::HSet(const std::string& key, const std::string& hkey, int64_t value){

    redisReply_ptr reply = Exec("HSET %s %s %ld", key.c_str(), hkey.c_str(), value);
    if(reply && reply->type == REDIS_REPLY_INTEGER){
        return reply->integer;
    }
    return -1;
}

int RedisConn::HSet(const std::string& key, const std::string& hkey, const std::string& value){

    redisReply_ptr reply = Exec("HSET %s %s %s", key.c_str(), hkey.c_str(), value.c_str());
    if(reply && reply->type == REDIS_REPLY_INTEGER){
        return reply->integer;
    }
    return -1;
}


// HGet
redisReply_ptr RedisConn::HGet(const std::string& key, const std::string& hkey) {
    redisReply_ptr reply = Exec("HGET %s %s", key.c_str(), hkey.c_str());
    if(!reply || reply->type == REDIS_REPLY_NIL) {
        reply.reset();
    }
    return reply;
}


bool RedisConn::HGet(const std::string& key, const std::string& hkey, int64_t& value) {

    redisReply_ptr reply = HGet(key, hkey);
    if(!reply){
        return false;
    }

    if(reply->type == REDIS_REPLY_STRING){
        value = ::atoll(reply->str);
        return true;
    } else if(reply->type == REDIS_REPLY_INTEGER) {
        value = reply->integer;
        return true;
    }

    log_err("unexpected reply->type: %d", reply->type);
    return false;
}

bool RedisConn::HGet(const std::string& key, const std::string& hkey, std::string& value) {

    redisReply_ptr reply = HGet(key, hkey);
    if(!reply){
        return false;
    }

    if(reply->type == REDIS_REPLY_STRING){
        value = reply->str;
        return true;
    } else if(reply->type == REDIS_REPLY_INTEGER) {
        value = local_convert_to_string(reply->integer);
        return true;
    }

    log_err("unexpected reply->type: %d", reply->type);
    return false;
}



// HGetAll
redisReply_ptr RedisConn::HGetAll(const std::string& key) {
    redisReply_ptr reply = Exec("HGETALL %s", key.c_str());
    return reply;
}

bool RedisConn::HGetAll(const std::string& key, std::vector<std::string>& values) {

    redisReply_ptr reply = HGetAll(key);
    if(!reply || reply->type != REDIS_REPLY_ARRAY) {
        return false;
    }

    for(size_t index = 0; index < reply->elements; index ++) {
        values.push_back(reply->element[index]->str);
    }

    return true;
}

bool RedisConn::HGetAll(const std::string& key, std::map<std::string, int64_t>& values) {

    redisReply_ptr reply = HGetAll(key);
    if(!reply || reply->type != REDIS_REPLY_ARRAY) {
        return false;
    }

    for(size_t index = 0; index < reply->elements; index += 2) {
        values[reply->element[index]->str] = ::atoll(reply->element[index + 1]->str);
    }

    return true;
}

bool RedisConn::HGetAll(const std::string& key, std::map<std::string, std::string>& values) {

    redisReply_ptr reply = HGetAll(key);
    if(!reply || reply->type != REDIS_REPLY_ARRAY) {
        return false;
    }

    for(size_t index = 0; index < reply->elements; index += 2) {
        values[reply->element[index]->str] = reply->element[index + 1]->str;
    }

    return true;
}


int RedisConn::LPush(const std::string& key, const std::string& value) {
    return listPush("LPUSH", key, value);
}

int RedisConn::LPush(const std::string& key, const std::vector<std::string>& values) {
    return listPush("LPUSH", key, values);
}

bool RedisConn::RPop(const std::string& key, std::string& value) {
    return listPop("RPOP", key, value);
}

int RedisConn::listPush(const char* cmd, const std::string& key, const std::string& value) {

    if(key.empty() || value.empty()) {
        return -1;
    }

    redisReply_ptr reply = Exec("%s %s %s", cmd, key.c_str(), value.c_str());
    if(reply && reply->type == REDIS_REPLY_INTEGER) {
        return reply->integer;
    }

    return -2;
}

int RedisConn::listPush(const char* cmd, const std::string& key, const std::vector<std::string>& values) {

    if(key.empty() || values.empty()) {
        return -1;
    }

    std::vector<const char*> argv;
    argv.reserve(1 + 1 + values.size());
    argv.push_back(cmd);
    argv.push_back(key.c_str());
    for(size_t index = 0; index < values.size(); index ++) {
        argv.push_back(values[index].c_str());
    }

    redisReply_ptr reply = ExecV(argv);
    if(reply && reply->type == REDIS_REPLY_INTEGER) {
        return reply->integer;
    }

    return -2;
}

bool RedisConn::listPop(const char* cmd, const std::string& key, std::string& value) {

    if(key.empty()) {
        return false;
    }

    redisReply_ptr reply = Exec("%s %s", cmd, key.c_str());
    if(!reply)
        return false;

    if(reply->type == REDIS_REPLY_INTEGER) {
        value = local_convert_to_string(reply->integer);
        return true;
    } else if(reply->type == REDIS_REPLY_STRING && reply->str != NULL) {
        value = reply->str;
        return true;
    } else if(reply->type == REDIS_REPLY_NIL) {
        // empty
        return false;
    }

    log_err("unknown reply->type: %d", reply->type);
    return false;
}



int RedisConn::LPush(const std::string& key, int64_t value) {
    return listPush("LPUSH", key, value);
}


bool RedisConn::RPop(const std::string& key, int64_t& value) {
    return listPop("RPOP", key, value);
}

int RedisConn::listPush(const char* cmd, const std::string& key, int64_t value) {

    if(key.empty()) {
        return -1;
    }

    redisReply_ptr reply = Exec("%s %s %ld", cmd, key.c_str(), value);
    if(reply && reply->type == REDIS_REPLY_INTEGER) {
        return reply->integer;
    }

    return -2;
}

bool RedisConn::listPop(const char* cmd, const std::string& key, int64_t& value) {

    if(key.empty()) {
        return false;
    }

    redisReply_ptr reply = Exec("%s %s", cmd, key.c_str());
    if(!reply)
        return false;

    if(reply->type == REDIS_REPLY_INTEGER) {
        value = reply->integer;
        return true;
    } else if(reply->type == REDIS_REPLY_STRING && reply->str != NULL) {
        value = ::atoll(reply->str);
        return true;
    } else if(reply->type == REDIS_REPLY_NIL) {
        // empty
        return false;
    }

    log_err("unknown reply->type: %d", reply->type);
    return false;
}





#if 0

void do_redis_test(redis_conn_ptr conn) {

#define CORE_OUT do { std::cout << __LINE__ << std::endl; ::abort(); } while(0)

    std::string key1 = "nccccc_test_key1";
    std::string key2 = "nccccc_test_key2";
    int64_t val1 = 123;
    std::string val2 = "s123";
    int64_t val1_r {};
    std::string val2_r {};

    conn->Set(key1, val1);
    conn->Set(key2, val2);

    if(!conn->Exists(key1))
        CORE_OUT;

    if(!conn->Exists(key2))
        CORE_OUT;

    if (!conn->Get(key1, val1_r) || val1 != val1_r) {
        CORE_OUT;
    }

    if (!conn->Get(key2, val2_r) || val2 != val2_r) {
        CORE_OUT;
    }

    std::vector<std::string> str_key = {key1, key2};
    std::vector<std::string> str_ret;
    if (!conn->MGet(str_key, str_ret)) {
        CORE_OUT;
    }

    if (str_ret.size() != str_key.size() ||
        str_ret[0] != "123" || str_ret[1] != val2) {
        CORE_OUT;
    }

    conn->Del(key1);
    if(conn->Exists(key1))
        CORE_OUT;


    conn->Set(key1, 100);
    conn->Incr(key1);

    {
        auto ret = conn->IncrBy(key1, 4);
        if (!ret || *ret != 105 ) {
            CORE_OUT;
        }

        auto ret2 = conn->DecrBy(key1, 4);
        if (!ret2 || *ret2 != 101 ) {
            CORE_OUT;
        }

        auto ret3 = conn->Decr(key1);
        if (!ret3 || *ret3 != 100 ) {
            CORE_OUT;
        }
    }


    std::string hkey = "nccc_hash_set";
    conn->HSet(hkey, key1, val1);
    conn->HSet(hkey, key2, val2);

    val1_r = 0; val2_r = "";
    if(!conn->HGet(hkey, key1, val1_r) || val1 != val1_r)
        CORE_OUT;

    if(!conn->HGet(hkey, key2, val2_r) || val2 != val2_r)
        CORE_OUT;

    std::map<std::string, std::string> map_ret2;
    if (!conn->HGetAll(hkey, map_ret2) || map_ret2.size() != 2) {
        CORE_OUT;
    }
    for (auto iter = map_ret2.begin(); iter != map_ret2.end(); ++ iter) {
        std::cout << iter->first << " - " << iter->second << std::endl;
    }

    std::string queue_name = "nccc_queue";
    std::string msg = "HAHA";
    int64_t     msg2 = 12987;
    conn->LPush(queue_name, msg);
    conn->LPush(queue_name, msg2);

    std::string msg_ret;
    int64_t msg2_ret;
    if (!conn->RPop(queue_name, msg_ret) || !conn->RPop(queue_name, msg2_ret)) {
        CORE_OUT;
    }

    if (msg != msg_ret || msg2 != msg2_ret) {
        CORE_OUT;
    }


    std::cout << "GOOD, test pass..." << std::endl;
    std::cin.get();
    ::exit(9);

#undef CORE_OUT
}

#endif