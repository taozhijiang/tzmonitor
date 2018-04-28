#ifndef __TNONBLOCKING_HELPER__
#define __TNONBLOCKING_HELPER__

#include "General.h"
#include <boost/make_shared.hpp>

#include <utils/Log.h>

#include <thrift/protocol/TBinaryProtocol.h>
#include <thrift/transport/TServerSocket.h>
#include <thrift/transport/TBufferTransports.h>
#include <thrift/server/TNonblockingServer.h>
#include <thrift/concurrency/PosixThreadFactory.h>


using namespace ::apache::thrift;

template <typename ServiceHandler, typename ServiceProcessor>
class TNonblockingHelper {
public:
    TNonblockingHelper(uint16_t port, uint16_t thread_sz, uint16_t io_thread_sz):
        port_(port), thread_sz_(thread_sz), io_thread_sz_(io_thread_sz) {
    }

    // do nothing currently
    bool init() {
        return true;
    }

    bool start_service() {

        if (!create_server()) {
            return false;
        }

        SAFE_ASSERT(threads_);
        SAFE_ASSERT(server_);

        try {
            threads_->start();
            server_->serve();
        } catch(...){
            log_err("Exception caught, failed to start_service");
            return false;
        }

        return true;
    }


    bool stop_service() {

        SAFE_ASSERT(server_);
        SAFE_ASSERT(threads_);

        try{
            server_->stop();

            threads_->stop();
            threads_->join();

            threads_.reset();
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

            // 协议层，使用Thrift私有的压缩数据格式
            boost::shared_ptr<protocol::TProtocolFactory> protocolFactory = boost::make_shared<protocol::TBinaryProtocolFactory>();

            // 工作线程组
            threads_ = concurrency::ThreadManager::newSimpleThreadManager(thread_sz_);
            boost::shared_ptr<concurrency::PosixThreadFactory> threadFactory = boost::make_shared<concurrency::PosixThreadFactory>();
            threads_->threadFactory(threadFactory);

            // 业务处理接口
            boost::shared_ptr<ServiceHandler> handler(new ServiceHandler());
            boost::shared_ptr<TProcessor> processor(new ServiceProcessor(handler));

            // Server层次
            boost::shared_ptr<server::TNonblockingServer> serverPtr(new server::TNonblockingServer(processor,  protocolFactory, port_, threads_));
            serverPtr->setNumIOThreads(io_thread_sz_);
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

#endif // __TNONBLOCKING_HELPER__