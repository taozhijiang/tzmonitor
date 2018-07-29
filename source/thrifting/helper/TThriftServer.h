/*-
 * Copyright (c) 2018 TAO Zhijiang<taozhijiang@gmail.com>
 *
 * Licensed under the BSD-3-Clause license, see LICENSE for full information.
 *
 */


#ifndef __TTHRIFT_SERVER_H__
#define __TTHRIFT_SERVER_H__

#include <memory>
#include <sstream>
#include <thread>

#include <boost/noncopyable.hpp>

template <typename ServiceHandler, typename ServiceProcessor>
class TThreadedHelper;

template <typename ServiceHandler, typename ServiceProcessor>
class TThreadPoolHelper;

template <typename ServiceHandler, typename ServiceProcessor>
class TNonblockingHelper;

// ServerHelperType for TNonblockingHelper, TThreadedHelper, TThreadPoolHelper
// For specific problem, do not need to impl all ServiceHelperType

// TThriftServer just forward operations to specified ServerType


template <typename ServiceHandler,
          typename ServiceProcessor,
          template <typename, typename> class ServerHelperType>
class TThriftServer: public boost::noncopyable {
public:
    TThriftServer(uint16_t port):
        running_(false) {
        server_impl_.reset(new ServerHelperType<ServiceHandler, ServiceProcessor>(port));
    }

    TThriftServer(uint16_t port, uint16_t thread_sz):
        running_(false) {
        server_impl_.reset(new ServerHelperType<ServiceHandler, ServiceProcessor>(port, thread_sz));
    }

    TThriftServer(uint16_t port, uint16_t thread_sz, uint16_t io_thread_sz):
        running_(false) {
        server_impl_.reset(new ServerHelperType<ServiceHandler, ServiceProcessor>(port, thread_sz, io_thread_sz));
    }

    bool init() {
        if (!server_impl_) {
            return false;
        }
        return server_impl_->init();
    }

    bool start_service() {
        if (!server_impl_ || running_) {
            log_err("ThriftService may already running...");
            return false;
        }

        thread_ptr_.reset(new std::thread(std::bind(&TThriftServer::run, this)));
        if (!thread_ptr_){
            log_err("ThriftService create thread failed ...");
            return false;
        }

        running_ = true;
        return true;
    }

    bool stop_service() {
        if (!server_impl_ || !running_) {
            log_err("ThriftService may already not running...");
            return false;
        }

        running_ = false;
        server_impl_->stop_service();

        thread_ptr_->join();

        return true;
    }

    virtual ~TThriftServer() {
        stop_service();
    }

private:

    // defer thread avoid blocking main
    void run() {

        log_notice("ThrifServer %#lx is running...", (long)pthread_self());

        SAFE_ASSERT(server_impl_);

        server_impl_->start_service();
    }

private:
    std::unique_ptr<ServerHelperType<ServiceHandler, ServiceProcessor>> server_impl_;
    std::shared_ptr<std::thread> thread_ptr_;
    bool running_;
};

#endif // __TTHRIFT_SERVER_H__
