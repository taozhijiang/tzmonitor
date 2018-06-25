#ifndef __TTHREAD_POOL_HELPER_H__
#define __TTHREAD_POOL_HELPER_H_ _

// Server thread goes back to the thread pool after client closes the connection for reuse.
// There is a limit on the number of threads. The thread pool won’t grow beyond the limit.

#include "General.h"

// Thrift严重依赖于boost库，所以这里的智能指针也适用boost的版本
#include <boost/make_shared.hpp>

#include <utils/Log.h>
#include "TThriftTypes.h"


template <typename ServiceHandler, typename ServiceProcessor>
class TThreadPoolHelper {
public:
    TThreadPoolHelper(uint16_t port, uint16_t thread_sz):
        port_(port), thread_sz_(thread_sz)  {
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
            // threads_->join();

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

            // use auto later

            // 协议层，使用Thrift私有的压缩数据格式
            boost::shared_ptr<ProtocolFactory> protocolFactory = boost::make_shared<ProtocolTypeFactory>();

            // 工作线程组
            threads_ = concurrency::ThreadManager::newSimpleThreadManager(thread_sz_);
            boost::shared_ptr<ThreadTypeFactory> threadFactory = boost::make_shared<ThreadTypeFactory>();
            threads_->threadFactory(threadFactory);

            // 业务处理接口
            boost::shared_ptr<ServiceHandler> handler = boost::make_shared<ServiceHandler>();
            boost::shared_ptr<TProcessor> processor = boost::make_shared<ServiceProcessor>(handler);

            // 传输层
            boost::shared_ptr<transport::TTransportFactory> transportFactory = boost::make_shared<TransportTypeFactory>();

            // Socket传输层
            boost::shared_ptr<transport::TServerTransport> tSocket = boost::make_shared<transport::TServerSocket>(port_);

            // Server层次
            boost::shared_ptr<server::TThreadPoolServer> serverPtr = boost::make_shared<server::TThreadPoolServer>(processor,  tSocket, transportFactory, protocolFactory, threads_);
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

    boost::shared_ptr<server::TServer> server_;
    boost::shared_ptr<concurrency::ThreadManager> threads_;
};

#endif // __TTHREAD_POOL_HELPER_H__