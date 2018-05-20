#define BOOST_TEST_MODULE redis_pool_test

#include <boost/test/unit_test.hpp>

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

    ::srand(::time(NULL));

    std::string redis_test_key1 = "redis_pool_prefix";
    std::string redis_test_key2 = "test_key_at_initialize";
    int64_t redis_test_value = ::random();
    int64_t redis_test_get   = 0;

    redisReply_ptr r = conn->exec("hmset %s %s %ld", redis_test_key1.c_str(), redis_test_key2.c_str(), redis_test_value);
    if (!r || r->type == REDIS_REPLY_ERROR) {
        BOOST_CHECK(false);
    }

    r = conn->exec("hmget %s %s", redis_test_key1.c_str(), redis_test_key2.c_str());
    if (!r || r->type == REDIS_REPLY_ERROR || r->elements != 1) {
        BOOST_CHECK(false);
    }

    std::string strNum(r->element[0]->str, r->element[0]->len);
    redis_test_get = ::atoll(strNum.c_str());

    if (redis_test_value != redis_test_get) {
        BOOST_CHECK(false);
    }

    std::cerr << "redis value: " << redis_test_get << std::endl;

    BOOST_CHECK(true);
}

BOOST_AUTO_TEST_SUITE_END()
