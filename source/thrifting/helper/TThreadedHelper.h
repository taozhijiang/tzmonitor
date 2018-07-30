/*-
 * Copyright (c) 2018 TAO Zhijiang<taozhijiang@gmail.com>
 *
 * Licensed under the BSD-3-Clause license, see LICENSE for full information.
 *
 */


#ifndef __TTHREADED_HELPER_H__
#define __TTHREADED_HELPER_H__


// Thrift严重依赖于boost库，所以这里的智能指针也适用boost的版本
#include <boost/make_shared.hpp>

#include <utils/Log.h>
#include "TThriftTypes.h"


template <typename ServiceHandler, typename ServiceProcessor>
class TThreadedHelper {
public:
   explicit TThreadedHelper(uint16_t port):
        port_(port) {
    }

    // do nothing currently
    bool init() {
        return true;
    }

    bool start_service() {

        if (!create_server()) {
            return false;
        }

        SAFE_ASSERT(server_);

        try {
            server_->serve();
        } catch(...){
            log_err("Exception caught, failed to start_service");
            return false;
        }

        return true;
    }


    bool stop_service() {

        SAFE_ASSERT(server_);

        try{
            server_->stop();
            server_.reset();
        } catch(...){
            log_err("Exception caught, failed to stop_service");
            return false;
        }

        return true;
    }

private:

    bool create_server() {

        try {

            // use auto later

            // 协议层，使用Thrift私有的压缩数据格式
            boost::shared_ptr<ProtocolFactory> protocolFactory = boost::make_shared<ProtocolTypeFactory>();

            // 业务处理接口
            boost::shared_ptr<ServiceHandler> handler = boost::make_shared<ServiceHandler>();
            boost::shared_ptr<TProcessor> processor = boost::make_shared<ServiceProcessor>(handler);

            // 传输层
            boost::shared_ptr<transport::TTransportFactory> transportFactory = boost::make_shared<TransportTypeFactory>();

            // Socket网络层
            boost::shared_ptr<transport::TServerTransport> tSocket = boost::make_shared<transport::TServerSocket>(port_);

            // Server
            boost::shared_ptr<server::TThreadedServer> serverPtr = boost::make_shared<server::TThreadedServer>(processor, tSocket, transportFactory, protocolFactory);
            server_ = serverPtr;

        } catch (...) {
            log_err("try create_server failed.");
            return false;
        }

        return true;
    }

private:
    uint16_t port_;
    uint16_t thread_sz_;
    uint16_t io_thread_sz_;

    boost::shared_ptr<server::TServer> server_;
    boost::shared_ptr<concurrency::ThreadManager> threads_;
};

#endif //  __TTHREADED_HELPER_H__
