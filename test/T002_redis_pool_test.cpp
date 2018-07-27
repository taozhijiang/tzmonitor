#define BOOST_TEST_MODULE redis_pool_test
#include <boost/test/included/unit_test.hpp>

#include <boost/algorithm/string.hpp>

#include "General.h"
#include <utils/Utils.h>
#include <connect/ConnPool.h>
#include <connect/RedisConn.h>


// 类似namespace的保护
BOOST_AUTO_TEST_SUITE(redis_pool_test)

BOOST_AUTO_TEST_CASE(redis_test)
{
    std::shared_ptr<ConnPool<RedisConn, RedisConnPoolHelper>> redis_pool_ptr;

    std::string config_file = "../tzmonitor.conf";
    if (!sys_config_init(config_file)) {
        BOOST_CHECK(false);
    }

    // Redis
    std::string redis_hostname;
    int redis_port;
    std::string redis_passwd;
    if (!get_config_value("redis.host_addr", redis_hostname) || !get_config_value("redis.host_port", redis_port)){
        BOOST_CHECK(false);
    }
    get_config_value("redis.passwd", redis_passwd);
    int redis_pool_size;
    if (!get_config_value("redis.conn_pool_size", redis_pool_size)) {
        redis_pool_size = 20;
    }

    RedisConnPoolHelper redis_helper(redis_hostname, redis_port, redis_passwd);
    redis_pool_ptr.reset(new ConnPool<RedisConn, RedisConnPoolHelper>("RedisPool", redis_pool_size, redis_helper, 60 /*60s*/));
    if (!redis_pool_ptr || !redis_pool_ptr->init()) {
        BOOST_CHECK(false);
    }

    redis_conn_ptr conn;
    redis_pool_ptr->request_scoped_conn(conn);

    if (!conn) {
        BOOST_CHECK(false);
    }

    ::srand((unsigned int)::time(NULL));

    std::string key1 = "nccccc_test_key1";
    std::string key2 = "nccccc_test_key2";
    int64_t val1 = 123;
    std::string val2 = "s123";
    int64_t val1_r {};
    std::string val2_r {};

    conn->Set(key1, val1);
    conn->Set(key2, val2);

    if(!conn->Exists(key1))
        BOOST_CHECK(false);

    if(!conn->Exists(key2))
        BOOST_CHECK(false);

    if (!conn->Get(key1, val1_r) || val1 != val1_r) {
        BOOST_CHECK(false);
    }

    if (!conn->Get(key2, val2_r) || val2 != val2_r) {
        BOOST_CHECK(false);
    }

    std::vector<std::string> str_key = {key1, key2};
    std::vector<std::string> str_ret;
    if (!conn->MGet(str_key, str_ret)) {
        BOOST_CHECK(false);
    }

    if (str_ret.size() != str_key.size() ||
        str_ret[0] != "123" || str_ret[1] != val2) {
        BOOST_CHECK(false);
    }

    conn->Del(key1);
    if(conn->Exists(key1))
        BOOST_CHECK(false);


    conn->Set(key1, 100);
    conn->Incr(key1);

    {
        auto ret = conn->IncrBy(key1, 4);
        if (!ret || *ret != 105 ) {
            BOOST_CHECK(false);
        }

        auto ret2 = conn->DecrBy(key1, 4);
        if (!ret2 || *ret2 != 101 ) {
            BOOST_CHECK(false);
        }

        auto ret3 = conn->Decr(key1);
        if (!ret3 || *ret3 != 100 ) {
            BOOST_CHECK(false);
        }
    }


    std::string hkey = "nccc_hash_set";
    conn->HSet(hkey, key1, val1);
    conn->HSet(hkey, key2, val2);

    val1_r = 0; val2_r = "";
    if(!conn->HGet(hkey, key1, val1_r) || val1 != val1_r)
        BOOST_CHECK(false);

    if(!conn->HGet(hkey, key2, val2_r) || val2 != val2_r)
        BOOST_CHECK(false);

    std::map<std::string, std::string> map_ret2;
    if (!conn->HGetAll(hkey, map_ret2) || map_ret2.size() != 2) {
        BOOST_CHECK(false);
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
        BOOST_CHECK(false);
    }

    if (msg != msg_ret || msg2 != msg2_ret) {
        BOOST_CHECK(false);
    }

    BOOST_CHECK(true);
}

BOOST_AUTO_TEST_SUITE_END()
