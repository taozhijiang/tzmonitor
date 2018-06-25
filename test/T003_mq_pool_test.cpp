#define BOOST_TEST_MODULE mq_pool_test
#include <boost/test/included/unit_test.hpp>

#include <boost/algorithm/string.hpp>

#include "General.h"

#include <connect/ConnPool.h>
#include <connect/MqConn.h>


// 类似namespace的保护
BOOST_AUTO_TEST_SUITE(mq_pool_test)

BOOST_AUTO_TEST_CASE(mq_test)
{

    std::shared_ptr<ConnPool<MqConn, MqConnPoolHelper>> mq_pool_ptr_;

    std::string connects = "amqp://tibank:%s@127.0.0.1:5672/tibank";
    std::string passwd   = "paybank";

    std::vector<std::string> vec;
    std::vector<std::string> realVec;
    boost::split(vec, connects, boost::is_any_of(";"));
    for (std::vector<std::string>::iterator it = vec.begin(); it != vec.cend(); ++it){
        std::string tmp = boost::trim_copy(*it);
        if (tmp.empty())
            continue;

        char connBuf[2048] = { 0, };
        snprintf(connBuf, sizeof(connBuf), tmp.c_str(), passwd.c_str());
        realVec.push_back(connBuf);
    }

    MqConnPoolHelper mq_helper(realVec, "paybank_exchange", "mqpooltest", "mqtest");
    mq_pool_ptr_.reset(new ConnPool<MqConn, MqConnPoolHelper>("MqPool", 7, mq_helper, 5*60 /*5min*/));
    if (!mq_pool_ptr_ || !mq_pool_ptr_->init()) {
        BOOST_CHECK(false);
    }

    std::string msg = "TAOZJFSFiMSG";
    std::string outmsg;

    mq_conn_ptr conn;
    mq_pool_ptr_->request_scoped_conn(conn);
    if (!conn) {
        BOOST_CHECK(false);
    }

    int ret = conn->publish(msg);
    if (ret != 0) {
        BOOST_CHECK(false);
    }

    ret = conn->get(outmsg);
    if (ret != 0) {
        BOOST_CHECK(false);
    }

    msg = "MSG233333";
    ret = conn->publish(msg);
    if (ret != 0) {
        BOOST_CHECK(false);
    }

    ret = conn->consume(outmsg, 5);
    if (ret != 0) {
        BOOST_CHECK(false);
    }

    BOOST_CHECK(true);
}

BOOST_AUTO_TEST_SUITE_END()
